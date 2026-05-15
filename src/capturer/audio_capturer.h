#ifndef AUDIO_CAPTURER_H_
#define AUDIO_CAPTURER_H_

#include <cstdint>
#include <memory>

#include "common/interface/subject.h"
#include "common/worker.h"

struct AudioBuffer {
    uint8_t *start;
    unsigned int length;
    unsigned int channels;
};

class AudioCapturer : public Subject<AudioBuffer> {
  public:
    AudioCapturer() = default;
    virtual ~AudioCapturer() = default;

    static constexpr int kChunkDurationMs = 10;

    int channels() const { return channels_; }
    int sample_rate() const { return sample_rate_; }
    int frames_per_chunk() const { return sample_rate_ * kChunkDurationMs / 1000; }

  protected:
    AudioCapturer(int channels, int sample_rate)
        : channels_(channels),
          sample_rate_(sample_rate) {}
    using Subject<AudioBuffer>::Next;
    virtual void StartCapture() = 0;

    int channels_ = 0;
    int sample_rate_ = 0;
    AudioBuffer shared_buffer_{};
    std::unique_ptr<Worker> worker_;
};

#endif
