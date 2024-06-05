#ifndef VIDEO_RECODER_H_
#define VIDEO_RECODER_H_

#include "args.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"
#include "recorder/recorder.h"

#include <string>
#include <future>
#include <queue>
#include <memory>
#include <mutex>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class VideoRecorder : public Recorder<Buffer> {
public:
    VideoRecorder(Args config, std::string encoder_name);
    virtual ~VideoRecorder() {};
    void Initialize() override;
    void OnBuffer(Buffer &buffer) override;
    void Pause() override;

protected:
    Args config;
    bool has_first_keyframe;
    std::string encoder_name;
    std::queue<Buffer> raw_buffer_queue;

    AVRational frame_rate;

    virtual void Encode(Buffer &buffer) = 0;

    void OnEncoded(Buffer buffer);
    void InitializeEncoder() override;
    void SetBaseTimestamp(struct timeval time);

private:
    struct timeval base_time_;
    std::mutex queue_mutex_;
    std::unique_ptr<Worker> worker_;

    void ConsumeBuffer();
};

#endif
