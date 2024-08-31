#ifndef VIDEO_RECODER_H_
#define VIDEO_RECODER_H_

#include <atomic>
#include <queue>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "args.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
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
    bool has_first_keyframe;
    std::string encoder_name;
    std::queue<rtc::scoped_refptr<V4l2FrameBuffer>> frame_buffer_queue;

    AVRational frame_rate;

    virtual void Encode(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) = 0;

    bool ConsumeBuffer() override;
    void OnEncoded(V4l2Buffer &buffer);
    void SetBaseTimestamp(struct timeval time);

  private:
    struct timeval base_time_;
    std::unique_ptr<V4l2Decoder> image_decoder_;

    void InitializeEncoderCtx(AVCodecContext *&encoder) override;
    void InitializeImageDecoder();
    std::string ReplaceExtension(const std::string &url, const std::string &new_extension);
};

#endif // VIDEO_RECODER_H_
