#ifndef OPENH264_RECORDER_H_
#define OPENH264_RECORDER_H_

#include "codecs/h264/openh264_encoder.h"
#include "recorder/video_recorder.h"

class Openh264Recorder : public VideoRecorder {
  public:
    static std::unique_ptr<Openh264Recorder> Create(Args config);
    Openh264Recorder(Args config, std::string encoder_name);

  protected:
    void ReleaseEncoder() override;
    void Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) override;

  private:
    std::unique_ptr<Openh264Encoder> encoder_;
};

#endif
