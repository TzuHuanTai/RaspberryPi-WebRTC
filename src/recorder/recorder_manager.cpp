#include "recorder/recorder_manager.h"

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>

#include "common/logging.h"
#include "common/utils.h"
#include "common/v4l2_frame_buffer.h"
#include "recorder/openh264_recorder.h"
#if defined(RPI_PLATFORM)
#include "recorder/raw_h264_recorder.h"
#include "recorder/v4l2_h264_recorder.h"
#endif

const int ROTATION_PERIOD = 60;
const unsigned long MIN_FREE_BYTE = 400 * 1024 * 1024;
const char *CONTAINER_FORMAT = "mp4";
const char *PREVIEW_IMAGE_EXTENSION = ".jpg";

AVFormatContext *RecUtil::CreateContainer(const std::string &full_path) {
    AVFormatContext *fmt_ctx = nullptr;

    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, CONTAINER_FORMAT, full_path.c_str()) <
        0) {
        ERROR_PRINT("Could not alloc output context");
        return nullptr;
    }

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, full_path.c_str(), AVIO_FLAG_WRITE) < 0) {
            ERROR_PRINT("Could not open %s", full_path.c_str());
            return nullptr;
        }
    }

    return fmt_ctx;
}

bool RecUtil::WriteFormatHeader(AVFormatContext *fmt_ctx) {
    if (avformat_write_header(fmt_ctx, nullptr) < 0) {
        ERROR_PRINT("Error occurred when opening output file");
        return false;
    }
    return true;
}

void RecUtil::CloseContext(AVFormatContext *fmt_ctx) {
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }
}

std::unique_ptr<RecorderManager> RecorderManager::Create(std::shared_ptr<VideoCapturer> video_src,
                                                         std::shared_ptr<PaCapturer> audio_src,
                                                         Args config) {
    auto instance = std::make_unique<RecorderManager>(config);

    if (video_src) {
        instance->CreateVideoRecorder(video_src);
        instance->SubscribeVideoSource(video_src);
    }
    if (audio_src) {
        instance->CreateAudioRecorder(audio_src);
        instance->SubscribeAudioSource(audio_src);
    }

    instance->worker_ = std::make_unique<Worker>("rotation_worker", [config]() {
        DEBUG_PRINT("Rotate files.");
        while (!Utils::CheckDriveSpace(config.record_path, MIN_FREE_BYTE)) {
            Utils::RotateFiles(config.record_path);
        }
        std::this_thread::sleep_for(std::chrono::seconds(ROTATION_PERIOD));
    });
    instance->worker_->Run();

    return instance;
}

void RecorderManager::CreateVideoRecorder(std::shared_ptr<VideoCapturer> capturer) {
    video_src_ = capturer;
    fps = capturer->fps();
    width = capturer->width();
    height = capturer->height();
    video_recorder = ([this, capturer]() -> std::unique_ptr<VideoRecorder> {
        if (config.record_mode == RecordMode::Snapshot) {
            return nullptr;
        }
#if defined(RPI_PLATFORM)
        if (capturer->format() == V4L2_PIX_FMT_H264) {
            return RawH264Recorder::Create(capturer->config());
        } else if (config.hw_accel) {
            return V4L2H264Recorder::Create(capturer->config());
        }
#endif

#if defined(JETSON_PLATFORM)
        // todo: use jetson hw video encoder
#endif
        return Openh264Recorder::Create(capturer->config());
    })();
}

void RecorderManager::CreateAudioRecorder(std::shared_ptr<PaCapturer> capturer) {
    audio_recorder = ([this, capturer]() -> std::unique_ptr<AudioRecorder> {
        if (config.record_mode == RecordMode::Snapshot) {
            return nullptr;
        } else {
            return AudioRecorder::Create(capturer->config());
        }
    })();
}

RecorderManager::RecorderManager(Args config)
    : config(config),
      fmt_ctx(nullptr),
      has_first_keyframe(false),
      record_path(config.record_path),
      elapsed_time_(0.0) {}

void RecorderManager::SubscribeVideoSource(std::shared_ptr<VideoCapturer> video_src) {
    video_subscription_ = video_src->Subscribe([this](rtc::scoped_refptr<V4L2FrameBuffer> buffer) {
        // waiting first keyframe to start recorders.
        if (!has_first_keyframe && ((buffer->flags() & V4L2_BUF_FLAG_KEYFRAME) ||
                                    video_src_->format() != V4L2_PIX_FMT_H264)) {
            Start();
            last_created_time_ = buffer->timestamp();
        }

        // restart to write in the new file.
        if (elapsed_time_ >= config.file_duration) {
            last_created_time_ = buffer->timestamp();
            Stop();
            Start();
        }

        if (has_first_keyframe && video_recorder) {
            video_recorder->OnBuffer(buffer);
        }

        elapsed_time_ = (buffer->timestamp().tv_sec - last_created_time_.tv_sec) +
                        (buffer->timestamp().tv_usec - last_created_time_.tv_usec) / 1000000.0;
    });

    if (video_recorder) {
        video_recorder->OnPacketed([this](AVPacket *pkt) {
            this->WriteIntoFile(pkt);
        });
    }
}

void RecorderManager::SubscribeAudioSource(std::shared_ptr<PaCapturer> audio_src) {
    if (!audio_recorder) {
        return;
    }

    audio_subscription_ = audio_src->Subscribe([this](PaBuffer buffer) {
        if (has_first_keyframe) {
            audio_recorder->OnBuffer(buffer);
        }
    });

    audio_recorder->OnPacketed([this](AVPacket *pkt) {
        this->WriteIntoFile(pkt);
    });
}

void RecorderManager::WriteIntoFile(AVPacket *pkt) {
    std::lock_guard<std::mutex> lock(ctx_mux);
    int ret;
    if (fmt_ctx && fmt_ctx->nb_streams > pkt->stream_index &&
        (ret = av_interleaved_write_frame(fmt_ctx, pkt)) < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error occurred: %s\n", err_buf);
    }
}

void RecorderManager::Start() {
    if (!Utils::CheckDriveSpace(record_path, MIN_FREE_BYTE)) {
        auto task = std::async(std::launch::async, [this]() {
            Utils::RotateFiles(record_path);
        });
    }

    FileInfo new_file(record_path, CONTAINER_FORMAT);
    auto folder = new_file.GetFolderPath();
    Utils::CreateFolder(folder);

    if (config.record_mode != RecordMode::Snapshot) {
        std::lock_guard<std::mutex> lock(ctx_mux);
        fmt_ctx = RecUtil::CreateContainer(new_file.GetFullPath());
        if (fmt_ctx == nullptr) {
            usleep(1000);
            return;
        }

        if (video_recorder) {
            video_recorder->AddStream(fmt_ctx);
        }
        if (audio_recorder) {
            audio_recorder->AddStream(fmt_ctx);
        }

        RecUtil::WriteFormatHeader(fmt_ctx);

        av_dump_format(fmt_ctx, 0, new_file.GetFullPath().c_str(), 1);
    }

    if (video_recorder) {
        video_recorder->Start();
    }
    if (audio_recorder) {
        audio_recorder->Start();
    }

    if (config.record_mode != RecordMode::Video) {
        auto image_path = ReplaceExtension(new_file.GetFullPath(), PREVIEW_IMAGE_EXTENSION);
        MakePreviewImage(image_path);
    }

    has_first_keyframe = true;
}

void RecorderManager::Stop() {
    if (video_recorder) {
        video_recorder->Stop();
    }
    if (audio_recorder) {
        audio_recorder->Stop();
    }

    if (fmt_ctx) {
        std::lock_guard<std::mutex> lock(ctx_mux);
        RecUtil::CloseContext(fmt_ctx);
        fmt_ctx = nullptr;
    }
}

RecorderManager::~RecorderManager() {
    printf("~RecorderManager\n");
    Stop();
    worker_.reset();
    video_recorder.reset();
    audio_recorder.reset();
}

void RecorderManager::MakePreviewImage(std::string path) {
    std::thread([this, path]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (video_src_ == nullptr) {
            return;
        }
        auto i420buff = video_src_->GetI420Frame();
        Utils::CreateJpegImage(i420buff->DataY(), i420buff->width(), i420buff->height(), path,
                               config.jpeg_quality);
    }).detach();
}

std::string RecorderManager::ReplaceExtension(const std::string &url,
                                              const std::string &new_extension) {
    size_t last_dot_pos = url.find_last_of('.');
    if (last_dot_pos == std::string::npos) {
        // No extension found, append the new extension
        return url + new_extension;
    } else {
        // Replace the existing extension
        return url.substr(0, last_dot_pos) + new_extension;
    }
}
