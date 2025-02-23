#ifndef VIDEO_RECORDER_H_
#define VIDEO_RECORDER_H_

#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "args.h"
#include "common/thread_safe_queue.h"
#include "common/v4l2_frame_buffer.h"
#include "recorder/recorder.h"
#include "v4l2_codecs/v4l2_decoder.h"

class VideoRecorder : public Recorder<V4l2Buffer> {
  public:
    VideoRecorder(Args config, std::string encoder_name);
    virtual ~VideoRecorder(){};
    void OnBuffer(V4l2Buffer &buffer) override;
    void PostStop() override;

  protected:
    Args config;
    std::atomic<bool> abort;
    std::string encoder_name;
    ThreadSafeQueue<rtc::scoped_refptr<V4l2FrameBuffer>> frame_buffer_queue;

    AVRational frame_rate;

    virtual void Encode(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) = 0;

    bool ConsumeBuffer() override;
    void OnEncoded(V4l2Buffer &buffer);
    void SetBaseTimestamp(struct timeval time);

  private:
    struct timeval base_time_;
    std::unique_ptr<V4l2Decoder> image_decoder_;

    void InitializeEncoderCtx(AVCodecContext *&encoder) override;
};

#endif // VIDEO_RECORDER_H_
