#ifndef OPENH264_ENCODER_
#define OPENH264_ENCODER_

#include <functional>

#include <api/video/i420_buffer.h>
#include <third_party/openh264/src/codec/api/wels/codec_api.h>

#include "args.h"
#include "common/v4l2_utils.h"

class Openh264Encoder {
  public:
    static std::unique_ptr<Openh264Encoder> Create(int width, int height, int fps);
    Openh264Encoder(int width, int height, int fps);
    ~Openh264Encoder();
    void Init();
    void Encode(rtc::scoped_refptr<webrtc::I420BufferInterface> frame_buffer,
                std::function<void(uint8_t *, int)> on_capture);

  private:
    int fps_;
    int width_;
    int height_;
    int bitrate_;
    ISVCEncoder *encoder_;
    SSourcePicture src_pic_;
};

#endif // OPENH264_ENCODER_
