#include "capturer/pa_capturer.h"

#include "common/logging.h"

#define BUFSIZE 1024
#define CHANNELS 2

std::shared_ptr<PaCapturer> PaCapturer::Create(Args args) {
    auto ptr = std::make_shared<PaCapturer>(args);
    ptr->CreateFloat32Source(args.sample_rate);
    ptr->StartCapture();
    return ptr;
}

PaCapturer::PaCapturer(Args args)
    : config_(args) {}

PaCapturer::~PaCapturer() {
    worker_.reset();
    if (src) {
        pa_simple_free(src);
    }
}

Args PaCapturer::config() const { return config_; }

void PaCapturer::CreateFloat32Source(int sample_rate) {
    int error;
    pa_sample_spec ss;
    ss.format = PA_SAMPLE_FLOAT32LE;
    ss.channels = CHANNELS;
    ss.rate = sample_rate;

    src = pa_simple_new(nullptr, "Microphone", PA_STREAM_RECORD, nullptr, "record", &ss, nullptr,
                        nullptr, &error);
    if (!src) {
        ERROR_PRINT("%s", pa_strerror(error));
        return;
    }
}

void PaCapturer::CaptureSamples() {
    int error;
    uint8_t buf[BUFSIZE * sizeof(float)];

    if (pa_simple_read(src, buf, sizeof(buf), &error) < 0) {
        if (src) {
            printf("pa_simple_read() failed: %s", pa_strerror(error));
            pa_simple_free(src);
        }
        return;
    }

    shared_buffer_ = {.start = buf, .length = BUFSIZE, .channels = CHANNELS};
    Next(shared_buffer_);
}

void PaCapturer::StartCapture() {
    worker_ = std::make_unique<Worker>("PaCapture", [this]() {
        CaptureSamples();
    });
    worker_->Run();
}
