#ifndef H264_RECORDER_H_
#define H264_RECORDER_H_

#include <atomic>

#include "codec/h264/h264_encoder.h"
#include "recorder/video_recorder.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_encoder.h"

class H264Recorder : public VideoRecorder {
  public:
    static std::unique_ptr<H264Recorder> Create(Args config);
    H264Recorder(Args config, std::string encoder_name);
    ~H264Recorder();
    void PreStart() override;
    void PostStop() override;

  protected:
    void Encode(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) override;

  private:
    std::mutex mutex_;
    std::unique_ptr<V4l2Decoder> decoder_;
    std::unique_ptr<V4l2Encoder> encoder_;
    std::unique_ptr<H264Encoder> sw_encoder_;

    void InitCodecs();
};

#endif // H264_RECORDER_H_
