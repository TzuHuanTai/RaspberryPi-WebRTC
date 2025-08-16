#include "recorder/openh264_recorder.h"

std::unique_ptr<Openh264Recorder> Openh264Recorder::Create(Args config) {
    return std::make_unique<Openh264Recorder>(config, "h264_v4l2m2m");
}

Openh264Recorder::Openh264Recorder(Args config, std::string encoder_name)
    : VideoRecorder(config, encoder_name) {}

void Openh264Recorder::Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (!encoder_) {
        encoder_ = Openh264Encoder::Create(config);
    }

    auto i420_buffer = frame_buffer->ToI420();
    encoder_->Encode(i420_buffer, [this, frame_buffer](uint8_t *encoded_buffer, int size) {
        OnEncoded(encoded_buffer, size, frame_buffer->timestamp());
    });
}

void Openh264Recorder::ReleaseEncoder() { encoder_.reset(); }
