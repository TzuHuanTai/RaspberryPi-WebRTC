#ifndef PA_CAPTURER_H_
#define PA_CAPTURER_H_

#include <pulse/error.h>
#include <pulse/simple.h>

#include "args.h"
#include "capturer/audio_capturer.h"
#include "common/worker.h"

class PaCapturer : public AudioCapturer {
  public:
    static std::shared_ptr<AudioCapturer> Create(Args args);
    PaCapturer();
    ~PaCapturer();
    void StartCapture() override;

  private:
    pa_simple *src;
    AudioBuffer shared_buffer_;
    std::unique_ptr<Worker> worker_;

    void CaptureSamples();
    void CreateFloat32Source(int sample_rate);
};

#endif
