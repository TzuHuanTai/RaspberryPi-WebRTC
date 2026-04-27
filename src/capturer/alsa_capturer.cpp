#include "capturer/alsa_capturer.h"

#include <cerrno>
#include <chrono>
#include <thread>

#include "common/logging.h"

std::shared_ptr<AudioCapturer> AlsaCapturer::Create(Args args) {
    auto ptr = std::make_shared<AlsaCapturer>();
    ptr->CreateFloat32Source(args.sample_rate);
    ptr->StartCapture();
    return ptr;
}

AlsaCapturer::AlsaCapturer()
    : pcm_handle_(nullptr),
      raw_capture_buffer_(kFramesPerBuffer * kChannels * sizeof(float)),
      float_capture_buffer_(kFramesPerBuffer * kChannels, 0.0f) {}

AlsaCapturer::~AlsaCapturer() {
    worker_.reset();
    if (pcm_handle_) {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
    }
}

void AlsaCapturer::CreateFloat32Source(int sample_rate) {
    const auto try_set_params = [this, sample_rate](snd_pcm_format_t alsa_fmt,
                                                    SampleFormat fmt) -> bool {
        const int ret = snd_pcm_set_params(pcm_handle_, alsa_fmt, SND_PCM_ACCESS_RW_INTERLEAVED,
                                           kChannels, sample_rate, 1, 500000);
        if (ret < 0) {
            return false;
        }
        sample_format_ = fmt;
        return true;
    };

    int ret = snd_pcm_open(&pcm_handle_, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0) {
        ERROR_PRINT("ALSA open failed: %s", snd_strerror(ret));
        pcm_handle_ = nullptr;
        return;
    }

    if (try_set_params(SND_PCM_FORMAT_FLOAT_LE, SampleFormat::Float32)) {
        INFO_PRINT("ALSA capture format: FLOAT_LE");
        return;
    }

    if (try_set_params(SND_PCM_FORMAT_S32_LE, SampleFormat::S32)) {
        INFO_PRINT("ALSA capture format fallback: S32_LE");
        return;
    }

    if (try_set_params(SND_PCM_FORMAT_S16_LE, SampleFormat::S16)) {
        INFO_PRINT("ALSA capture format fallback: S16_LE");
        return;
    }

    ERROR_PRINT("ALSA set params failed for FLOAT_LE/S32_LE/S16_LE");
    if (pcm_handle_) {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
    }
}

void AlsaCapturer::CaptureSamples() {
    if (!pcm_handle_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return;
    }

    const int wait_res = snd_pcm_wait(pcm_handle_, 1000);
    if (wait_res == 0) {
        // No data within timeout, avoid spinning.
        return;
    }
    if (wait_res < 0) {
        if (wait_res == -EPIPE) {
            snd_pcm_prepare(pcm_handle_);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return;
    }

    const size_t bytes_per_sample =
        sample_format_ == SampleFormat::S16 ? sizeof(int16_t) : sizeof(int32_t);
    const size_t bytes_per_frame = bytes_per_sample * kChannels;
    const size_t read_size = kFramesPerBuffer * bytes_per_frame;
    if (raw_capture_buffer_.size() < read_size) {
        raw_capture_buffer_.resize(read_size);
    }

    const auto frames_read =
        snd_pcm_readi(pcm_handle_, raw_capture_buffer_.data(), kFramesPerBuffer);
    if (frames_read == -EAGAIN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return;
    }
    if (frames_read == -EPIPE) {
        snd_pcm_prepare(pcm_handle_);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return;
    }

    if (frames_read < 0) {
        ERROR_PRINT("ALSA read failed: %s", snd_strerror(static_cast<int>(frames_read)));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return;
    }

    const size_t sample_count = static_cast<size_t>(frames_read) * kChannels;
    if (float_capture_buffer_.size() < sample_count) {
        float_capture_buffer_.resize(sample_count);
    }

    if (sample_format_ == SampleFormat::Float32) {
        auto *src = reinterpret_cast<const float *>(raw_capture_buffer_.data());
        std::copy(src, src + sample_count, float_capture_buffer_.begin());
    } else if (sample_format_ == SampleFormat::S32) {
        constexpr float scale = 1.0f / 2147483648.0f;
        auto *src = reinterpret_cast<const int32_t *>(raw_capture_buffer_.data());
        for (size_t i = 0; i < sample_count; ++i) {
            float_capture_buffer_[i] = static_cast<float>(src[i]) * scale;
        }
    } else {
        constexpr float scale = 1.0f / 32768.0f;
        auto *src = reinterpret_cast<const int16_t *>(raw_capture_buffer_.data());
        for (size_t i = 0; i < sample_count; ++i) {
            float_capture_buffer_[i] = static_cast<float>(src[i]) * scale;
        }
    }

    shared_buffer_ = {.start = reinterpret_cast<uint8_t *>(float_capture_buffer_.data()),
                      .length = static_cast<unsigned int>(sample_count),
                      .channels = kChannels};
    Next(shared_buffer_);
}

void AlsaCapturer::StartCapture() {
    worker_ = std::make_unique<Worker>("AlsaCapture", [this]() {
        CaptureSamples();
    });
    worker_->Run();
}
