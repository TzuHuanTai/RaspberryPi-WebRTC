#ifndef PA_CAPTURER_H_
#define PA_CAPTURER_H_

#include <pulse/error.h>
#include <pulse/simple.h>

#include "args.h"
#include "common/interface/subject.h"
#include "common/worker.h"

struct PaBuffer {
    uint8_t *start;
    unsigned int length;
    unsigned int channels;
};

class PaCapturer : public Subject<PaBuffer> {
  public:
    static std::shared_ptr<PaCapturer> Create(Args args);
    PaCapturer(Args args);
    ~PaCapturer();
    Args config() const;
    void StartCapture();

  protected:
    using Subject<PaBuffer>::Next;

  private:
    Args config_;
    pa_simple *src;
    PaBuffer shared_buffer_;
    std::unique_ptr<Worker> worker_;

    void CaptureSamples();
    void CreateFloat32Source(int sample_rate);
};

#endif
