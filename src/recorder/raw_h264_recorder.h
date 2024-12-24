#ifndef RAW_H264_RECORDER_H_
#define RAW_H264_RECORDER_H_

#include "recorder/video_recorder.h"

class RawH264Recorder : public VideoRecorder {
  public:
    static std::unique_ptr<RawH264Recorder> Create(Args config);
    RawH264Recorder(Args config, std::string encoder_name);
    ~RawH264Recorder();
    void PreStart() override;
    void PostStop() override;
    bool CheckNALUnits(const V4l2Buffer &buffer);

  protected:
    void Encode(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) override;

  private:
    bool has_sps_;
    bool has_pps_;
    bool has_first_keyframe_;
};

#endif // RAW_H264_RECORDER_H_
