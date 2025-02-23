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

void H264Recorder::Encode(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (abort) {
        return;
    }

    auto i420_buffer = frame_buffer->ToI420();
    if (config.hw_accel) {
        unsigned int i420_buffer_size =
            (i420_buffer->StrideY() * frame_buffer->height()) +
            ((i420_buffer->StrideY() + 1) / 2) * ((frame_buffer->height() + 1) / 2) * 2;

        V4l2Buffer decoded_buffer((void *)i420_buffer->DataY(), i420_buffer_size);

        encoder_->EmplaceBuffer(decoded_buffer, [this, frame_buffer](V4l2Buffer encoded_buffer) {
            encoded_buffer.timestamp = frame_buffer->timestamp();
            OnEncoded(encoded_buffer);
        });
    } else {
        sw_encoder_->Encode(i420_buffer, [this, frame_buffer](uint8_t *encoded_buffer, int size) {
            V4l2Buffer buffer((void *)encoded_buffer, size, frame_buffer->flags(),
                              frame_buffer->timestamp());
            OnEncoded(buffer);
        });
    }
}

void H264Recorder::PreStart() { InitCodecs(); }

void H264Recorder::PostStop() {
    std::lock_guard<std::mutex> lock(mutex_);
    abort = true;
    encoder_.reset();
    sw_encoder_.reset();
}

void H264Recorder::InitCodecs() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config.hw_accel) {
        encoder_ = V4l2Encoder::Create(config.width, config.height, false);
        encoder_->SetFps(config.fps);
        encoder_->SetBitrate(config.width * config.height * config.fps * 0.1);
        V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                             V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_H264_LEVEL,
                             V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
        V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
        V4l2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 60);
    } else {
        sw_encoder_ = H264Encoder::Create(config);
    }
}
