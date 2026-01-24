#ifndef V4L2_H264_RECORDER_H_
#define V4L2_H264_RECORDER_H_

#include "codecs/v4l2/v4l2_encoder.h"
#include "recorder/video_recorder.h"

class V4L2H264Recorder : public VideoRecorder {
  public:
    static std::unique_ptr<V4L2H264Recorder> Create(int width, int height, int fps);
    V4L2H264Recorder(int width, int height, int fps);

  protected:
    void ReleaseEncoder() override;
    void Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) override;

  private:
    std::unique_ptr<V4L2Encoder> encoder_;
};

#endif
