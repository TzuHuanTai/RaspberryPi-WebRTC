#include "recorder/v4l2_h264_recorder.h"

std::unique_ptr<V4L2H264Recorder> V4L2H264Recorder::Create(Args config) {
    return std::make_unique<V4L2H264Recorder>(config, "h264_v4l2m2m");
}

V4L2H264Recorder::V4L2H264Recorder(Args config, std::string encoder_name)
    : VideoRecorder(config, encoder_name) {}

void V4L2H264Recorder::Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (!encoder_) {
        encoder_ = V4L2Encoder::Create(config.width, config.height, frame_buffer->format(), false);
        encoder_->SetFps(config.fps);
        encoder_->SetBitrate(config.width * config.height * config.fps * 0.1);
        encoder_->SetRateControlMode(V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        encoder_->SetLevel(V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
        encoder_->ForceKeyFrame();
        encoder_->SetIFrameInterval(30);
    }

    encoder_->EmplaceBuffer(frame_buffer, [this, frame_buffer](V4L2FrameBufferRef encoded_buffer) {
        OnEncoded((uint8_t *)encoded_buffer->Data(), encoded_buffer->size(),
                  frame_buffer->timestamp());
    });
}

void V4L2H264Recorder::ReleaseEncoder() { encoder_.reset(); }
