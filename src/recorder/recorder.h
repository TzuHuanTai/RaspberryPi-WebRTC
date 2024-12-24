#ifndef RECORDER_H_
#define RECORDER_H_

#include <condition_variable>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "common/interface/subject.h"
#include "common/worker.h"

template <typename T> class Recorder {
  public:
    using OnPacketedFunc = std::function<void(AVPacket *pkt)>;

    Recorder() = default;
    ~Recorder() { avcodec_free_context(&encoder); };

    virtual void OnBuffer(T &buffer) = 0;

    void Initialize() {
        avcodec_free_context(&encoder);
        InitializeEncoderCtx(encoder);
        worker = std::make_unique<Worker>("Recorder", [this]() {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_var_.wait_for(lock, std::chrono::milliseconds(10), [this] {
                return abort_.load();
            });
            ConsumeBuffer();
        });
    }

    virtual void PostStop(){};
    virtual void PreStart(){};

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        return st != nullptr;
    }

    void OnPacketed(OnPacketedFunc fn) { on_packeted = fn; }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            abort_.store(true);
        }
        cond_var_.notify_all();
        worker.reset();
        PostStop();
    }

    void Start() {
        Initialize();
        PreStart();
        abort_.store(false);
        worker->Run();
    }

  protected:
    OnPacketedFunc on_packeted;
    std::unique_ptr<Worker> worker;
    AVCodecContext *encoder;
    AVStream *st;

    virtual void InitializeEncoderCtx(AVCodecContext *&encoder) = 0;
    virtual bool ConsumeBuffer() = 0;
    void OnPacketed(AVPacket *pkt) {
        if (on_packeted) {
            on_packeted(pkt);
        }
    }

  private:
    std::atomic<bool> abort_;
    std::mutex mtx_;
    std::condition_variable cond_var_;
};

#endif // RECORDER_H_
