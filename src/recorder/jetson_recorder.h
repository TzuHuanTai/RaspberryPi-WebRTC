#ifndef V4L2_H264_RECORDER_H_
#define V4L2_H264_RECORDER_H_

#include "codecs/jetson/jetson_encoder.h"
#include "recorder/video_recorder.h"

class JetsonRecorder : public VideoRecorder {
  public:
    static std::unique_ptr<JetsonRecorder> Create(int width, int height, int fps);
    JetsonRecorder(int width, int height, int fps);

  protected:
    void ReleaseEncoder() override;
    void Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) override;

  private:
    std::unique_ptr<JetsonEncoder> encoder_;
};

#endif
