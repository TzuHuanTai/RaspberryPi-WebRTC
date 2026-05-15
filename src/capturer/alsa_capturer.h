#ifndef ALSA_CAPTURER_H_
#define ALSA_CAPTURER_H_

#include <alsa/asoundlib.h>

#include <cstdint>
#include <vector>

#include "args.h"
#include "capturer/audio_capturer.h"

class AlsaCapturer : public AudioCapturer {
  public:
    static std::shared_ptr<AudioCapturer> Create(Args args);

    AlsaCapturer(Args args);
    ~AlsaCapturer();

  protected:
    void StartCapture() override;

  private:
    enum class SampleFormat {
        Float32,
        S32,
        S16,
    };

    snd_pcm_t *pcm_handle_;
    SampleFormat sample_format_ = SampleFormat::Float32;
    std::vector<uint8_t> raw_capture_buffer_;
    std::vector<float> float_capture_buffer_;

    bool CreateFloat32Source();
    void CaptureSamples();
};

#endif
