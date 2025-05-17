#ifndef RAW_H264_RECORDER_H_
#define RAW_H264_RECORDER_H_

#include "recorder/video_recorder.h"

class RawH264Recorder : public VideoRecorder {
  public:
    static std::unique_ptr<RawH264Recorder> Create(Args config);
    RawH264Recorder(Args config, std::string encoder_name);
    ~RawH264Recorder();
    void OnStart() override;

  protected:
    void ReleaseEncoder() override;
    void Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) override;

  private:
    bool has_sps_;
    bool has_pps_;
    bool has_first_keyframe_;

    bool CheckNALUnits(const void *start, unsigned int length);
};

#endif // RAW_H264_RECORDER_H_
