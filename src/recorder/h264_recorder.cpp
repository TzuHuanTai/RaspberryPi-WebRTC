#include "recorder/h264_recorder.h"

std::unique_ptr<H264Recorder> H264Recorder::Create(Args config) {
    return std::make_unique<H264Recorder>(config, "h264_v4l2m2m");
}

H264Recorder::H264Recorder(Args config, std::string encoder_name)
    : VideoRecorder(config, encoder_name) {}

H264Recorder::~H264Recorder() {
    encoder_.reset();
    sw_encoder_.reset();
}

void H264Recorder::Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (config.hw_accel && !encoder_) {
        encoder_ = V4L2Encoder::Create(config.width, config.height, frame_buffer->format(), false);
        encoder_->SetFps(config.fps);
        encoder_->SetBitrate(config.width * config.height * config.fps * 0.1);
        V4L2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                             V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        V4L2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_H264_LEVEL,
                             V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
        V4L2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
        V4L2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 30);
    } else if (!sw_encoder_) {
        sw_encoder_ = Openh264Encoder::Create(config);
    }

    if (config.hw_accel) {
        auto decoded_buffer =
            V4L2Buffer::FromRaw(const_cast<void *>(frame_buffer->Data()), frame_buffer->size());
        encoder_->EmplaceBuffer(decoded_buffer, [this, frame_buffer](V4L2Buffer encoded_buffer) {
            OnEncoded(static_cast<uint8_t *>(encoded_buffer.start), encoded_buffer.length,
                      frame_buffer->timestamp());
        });
    } else {
        auto i420_buffer = frame_buffer->ToI420();
        sw_encoder_->Encode(i420_buffer, [this, frame_buffer](uint8_t *encoded_buffer, int size) {
            OnEncoded(encoded_buffer, size, frame_buffer->timestamp());
        });
    }
}

void H264Recorder::ReleaseEncoder() {
    if (config.hw_accel) {
        encoder_.reset();
    } else {
        sw_encoder_.reset();
    }
}
