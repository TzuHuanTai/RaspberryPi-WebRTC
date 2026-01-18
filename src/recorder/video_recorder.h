#ifndef VIDEO_RECORDER_H_
#define VIDEO_RECORDER_H_

#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "args.h"
#include "codecs/v4l2/v4l2_decoder.h"
#include "common/thread_safe_queue.h"
#include "common/v4l2_frame_buffer.h"
#include "recorder/recorder.h"

class VideoRecorder : public Recorder<rtc::scoped_refptr<V4L2FrameBuffer>> {
  public:
    VideoRecorder(int width, int height, int fps, std::string encoder_name);
    virtual ~VideoRecorder(){};
    void OnBuffer(rtc::scoped_refptr<V4L2FrameBuffer> buffer) override;
    void OnStop() override final;

  protected:
    int fps;
    int width;
    int height;
    std::string encoder_name;
    ThreadSafeQueue<rtc::scoped_refptr<V4L2FrameBuffer>> frame_buffer_queue;

    AVRational frame_rate;

    virtual void ReleaseEncoder() = 0;
    virtual void Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) = 0;

    bool ConsumeBuffer() override;
    void OnEncoded(uint8_t *start, uint32_t length, timeval timestamp);
    void SetBaseTimestamp(struct timeval time);

  private:
    std::mutex encoder_mtx_;
    std::atomic<bool> abort_;
    struct timeval base_time_;
    std::unique_ptr<V4L2Decoder> image_decoder_;

    void InitializeEncoderCtx(AVCodecContext *&encoder) override;
};

#endif // VIDEO_RECORDER_H_
