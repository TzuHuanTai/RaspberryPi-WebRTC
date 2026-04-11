#include "recorder/openh264_recorder.h"

std::unique_ptr<Openh264Recorder> Openh264Recorder::Create(int width, int height, int fps) {
    return std::make_unique<Openh264Recorder>(width, height, fps);
}

Openh264Recorder::Openh264Recorder(int width, int height, int fps)
    : VideoRecorder(width, height, fps, AV_CODEC_ID_H264) {}

void Openh264Recorder::Encode(V4L2FrameBufferRef frame_buffer) {
    if (!encoder_) {
        EncoderConfig config = {
            .width = width,
            .height = height,
            .fps = fps,
            .bitrate = static_cast<int>(width * height * fps * 0.1),
            .keyframe_interval = fps,
            .rc_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
        };
        encoder_ = Openh264Encoder::Create(config);
    }

    auto i420_buffer = frame_buffer->ToI420();
    encoder_->Encode(i420_buffer,
                     [this, frame_buffer](uint8_t *encoded_buffer, int size, bool is_keyframe) {
                         uint32_t flags = is_keyframe ? V4L2_BUF_FLAG_KEYFRAME : 0;
                         OnEncoded(encoded_buffer, size, frame_buffer->timestamp(), flags);
                     });
}

void Openh264Recorder::ReleaseEncoder() { encoder_.reset(); }
