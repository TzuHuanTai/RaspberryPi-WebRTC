#ifndef ALSA_CAPTURER_H_
#define ALSA_CAPTURER_H_

#include <alsa/asoundlib.h>

#include <vector>

#include "args.h"
#include "capturer/audio_capturer.h"
#include "common/worker.h"

class AlsaCapturer : public AudioCapturer {
  public:
    static std::shared_ptr<AudioCapturer> Create(Args args);
    AlsaCapturer();
    ~AlsaCapturer();
    void StartCapture() override;

  private:
    enum class SampleFormat {
        Float32,
        S32,
        S16,
    };

    static constexpr int kChannels = 2;
    static constexpr snd_pcm_uframes_t kFramesPerBuffer = 1024;

    snd_pcm_t *pcm_handle_;
    SampleFormat sample_format_ = SampleFormat::Float32;
    AudioBuffer shared_buffer_;
    std::vector<uint8_t> raw_capture_buffer_;
    std::vector<float> float_capture_buffer_;
    std::unique_ptr<Worker> worker_;

    void CreateFloat32Source(int sample_rate);
    void CaptureSamples();
};

#endif
