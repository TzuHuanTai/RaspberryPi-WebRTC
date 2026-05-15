#ifndef PA_CAPTURER_H_
#define PA_CAPTURER_H_

#include <vector>

#include <pulse/error.h>
#include <pulse/simple.h>

#include "args.h"
#include "capturer/audio_capturer.h"

class PaCapturer : public AudioCapturer {
  public:
    static std::shared_ptr<AudioCapturer> Create(Args args);

    PaCapturer(Args args);
    ~PaCapturer();

  protected:
    void StartCapture() override;

  private:
    pa_simple *src;
    std::vector<uint8_t> capture_buf_;

    void CaptureSamples();
    bool CreateFloat32Source();
};

#endif
