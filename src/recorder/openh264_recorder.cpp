#include "recorder/openh264_recorder.h"

std::unique_ptr<Openh264Recorder> Openh264Recorder::Create(int width, int height, int fps) {
    return std::make_unique<Openh264Recorder>(width, height, fps);
}

Openh264Recorder::Openh264Recorder(int width, int height, int fps)
    : VideoRecorder(width, height, fps, AV_CODEC_ID_H264) {}

void Openh264Recorder::Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (!encoder_) {
        encoder_ = Openh264Encoder::Create(width, height, fps);
    }

    auto i420_buffer = frame_buffer->ToI420();
    encoder_->Encode(i420_buffer, [this, frame_buffer](uint8_t *encoded_buffer, int size) {
        OnEncoded(encoded_buffer, size, frame_buffer->timestamp());
    });
}

void Openh264Recorder::ReleaseEncoder() { encoder_.reset(); }
