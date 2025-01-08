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
    ~Recorder() { Stop(); };

    virtual void OnBuffer(T &buffer) = 0;

    virtual void PostStop(){};
    virtual void PreStart(){};

    bool AddStream(AVFormatContext *output_fmt_ctx) {
        avcodec_free_context(&encoder);
        InitializeEncoderCtx(encoder);
        st = avformat_new_stream(output_fmt_ctx, encoder->codec);
        avcodec_parameters_from_context(st->codecpar, encoder);

        return st != nullptr;
    }

    void OnPacketed(OnPacketedFunc fn) { on_packeted = fn; }

    void Stop() {
        worker.reset();
        avcodec_free_context(&encoder);
        PostStop();
    }

    void Start() {
        worker = std::make_unique<Worker>("Recorder", [this]() {
            ConsumeBuffer();
        });
        PreStart();
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
};

#endif // RECORDER_H_
