#ifndef AUDIO_CAPTURER_H_
#define AUDIO_CAPTURER_H_

#include "common/interface/subject.h"

struct AudioBuffer {
    std::uint8_t *start;
    unsigned int length;
    unsigned int channels;
};

class AudioCapturer : public Subject<AudioBuffer> {
  public:
    AudioCapturer() = default;
    virtual ~AudioCapturer() = default;

    virtual void StartCapture() = 0;

  protected:
    using Subject<AudioBuffer>::Next;
};

#endif
