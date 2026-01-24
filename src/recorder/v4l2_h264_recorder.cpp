#include "recorder/v4l2_h264_recorder.h"

std::unique_ptr<V4L2H264Recorder> V4L2H264Recorder::Create(int width, int height, int fps) {
    return std::make_unique<V4L2H264Recorder>(width, height, fps);
}

V4L2H264Recorder::V4L2H264Recorder(int width, int height, int fps)
    : VideoRecorder(width, height, fps, AV_CODEC_ID_H264) {}

void V4L2H264Recorder::Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (!encoder_) {
        encoder_ = V4L2Encoder::Create(width, height, frame_buffer->format(), false);
        encoder_->SetFps(fps);
        encoder_->SetBitrate(width * height * fps * 0.1);
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
