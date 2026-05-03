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
#include "recorder/raw_h264_recorder.h"
#if defined(USE_RPI_HW_ENCODER)
#include "recorder/v4l2_h264_recorder.h"
#elif defined(USE_JETSON_HW_ENCODER)
#include "recorder/jetson_recorder.h"
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

void RecUtil::CloseContext(AVFormatContext *fmt_ctx) {
    if (fmt_ctx) {
        av_write_trailer(fmt_ctx);
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&fmt_ctx->pb);
        }
        avformat_free_context(fmt_ctx);
    }
}

std::unique_ptr<RecorderManager> RecorderManager::Create(std::shared_ptr<VideoCapturer> video_src,
                                                         std::shared_ptr<AudioCapturer> audio_src,
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

std::shared_ptr<RecorderManager>
RecorderManager::CreateOnDemand(std::shared_ptr<VideoCapturer> video_src,
                                std::shared_ptr<AudioCapturer> audio_src, Args config) {
    auto instance = std::make_shared<RecorderManager>(config);
    instance->auto_start_ = false;

    if (video_src) {
        instance->CreateVideoRecorder(video_src);
        instance->SubscribeVideoSource(video_src);
    }
    if (audio_src) {
        instance->CreateAudioRecorder(audio_src);
        instance->SubscribeAudioSource(audio_src);
    }

    instance->worker_ = std::make_unique<Worker>("ondemand_rotation_worker", [config]() {
        DEBUG_PRINT("Rotate on-demand files.");
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
    width = capturer->width(config.record_stream_idx);
    height = capturer->height(config.record_stream_idx);
    video_recorder = ([this, capturer]() -> std::unique_ptr<VideoRecorder> {
        if (config.record_type == RecordType::Snapshot) {
            return nullptr;
        }
        if (capturer->format() == V4L2_PIX_FMT_H264) {
            return RawH264Recorder::Create(width, height, fps);
        } else if (config.hw_accel) {
#if defined(USE_RPI_HW_ENCODER)
            return V4L2H264Recorder::Create(width, height, fps);
#elif defined(USE_JETSON_HW_ENCODER)
            return JetsonRecorder::Create(width, height, fps);
#endif
        }
        return Openh264Recorder::Create(width, height, fps);
    })();
}

void RecorderManager::CreateAudioRecorder(std::shared_ptr<AudioCapturer> audio_src) {
    audio_recorder = ([this, &audio_src]() -> std::unique_ptr<AudioRecorder> {
        if (config.record_type == RecordType::Snapshot) {
            return nullptr;
        } else {
            return AudioRecorder::Create(audio_src->sample_rate());
        }
    })();
}

RecorderManager::RecorderManager(Args config)
    : config(config),
      fmt_ctx(nullptr),
      auto_start_(true),
      header_written_(false),
      has_first_keyframe(false),
      time_reset_pending_(false),
      record_path(config.record_path),
      elapsed_time_(0.0) {}

void RecorderManager::SubscribeVideoSource(std::shared_ptr<VideoCapturer> video_src) {
    video_subscription_ = video_src->Subscribe(
        [this](V4L2FrameBufferRef buffer) {
            // waiting first keyframe to start recorders.
            if (auto_start_ && !has_first_keyframe &&
                ((buffer->flags() & V4L2_BUF_FLAG_KEYFRAME) ||
                 video_src_->format() != V4L2_PIX_FMT_H264)) {
                Start();
                last_created_time_ = buffer->timestamp();
            }

            // restart to write in the new file on the next keyframe boundary.
            if (has_first_keyframe && elapsed_time_ >= config.file_duration &&
                ((buffer->flags() & V4L2_BUF_FLAG_KEYFRAME) ||
                 video_src_->format() != V4L2_PIX_FMT_H264)) {
                Stop();
                Start();
            }

            if (has_first_keyframe && video_recorder) {
                video_recorder->OnBuffer(buffer);
            }

            // Sync last_created_time_ to V4L2 time base on first callback after Start()
            if (time_reset_pending_) {
                last_created_time_ = buffer->timestamp();
                elapsed_time_ = 0.0;
                time_reset_pending_ = false;
            } else {
                int64_t elapsed_us =
                    (int64_t)(buffer->timestamp().tv_sec - last_created_time_.tv_sec) * 1000000LL +
                    (int64_t)(buffer->timestamp().tv_usec - last_created_time_.tv_usec);
                elapsed_time_ = elapsed_us / 1e6;
            }
        },
        config.record_stream_idx);

    if (video_recorder) {
        video_recorder->OnPacketed([this](AVPacket *pkt) {
            this->WriteIntoFile(pkt);
        });
    }
}

void RecorderManager::SubscribeAudioSource(std::shared_ptr<AudioCapturer> audio_src) {
    if (!audio_recorder) {
        return;
    }

    audio_subscription_ = audio_src->Subscribe([this](AudioBuffer buffer) {
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

    if (!fmt_ctx || !has_first_keyframe)
        return;

    if (!header_written_) {
        if (!(pkt->flags & AV_PKT_FLAG_KEY)) {
            return;
        }

        AVCodecParameters *codecpar = fmt_ctx->streams[pkt->stream_index]->codecpar;

        if (codecpar->codec_id == AV_CODEC_ID_AV1) {
            av_free(codecpar->extradata);
            codecpar->extradata = (uint8_t *)av_malloc(pkt->size + AV_INPUT_BUFFER_PADDING_SIZE);
            memcpy(codecpar->extradata, pkt->data, pkt->size);
            memset(codecpar->extradata + pkt->size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
            codecpar->extradata_size = pkt->size;
        }

        if (avformat_write_header(fmt_ctx, nullptr) < 0) {
            ERROR_PRINT("Error writing header, close context to avoid corrupt muxer state");
            avio_closep(&fmt_ctx->pb);
            avformat_free_context(fmt_ctx);
            fmt_ctx = nullptr;
            return;
        }
        header_written_ = true;
    }

    int ret;
    if (fmt_ctx->nb_streams > pkt->stream_index &&
        (ret = av_interleaved_write_frame(fmt_ctx, pkt)) < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error occurred: %s\n", err_buf);
    }
}

void RecorderManager::Start() {
    if (!Utils::CheckDriveSpace(record_path, MIN_FREE_BYTE)) {
        Utils::RotateFiles(record_path);
    }

    FileInfo new_file(record_path, CONTAINER_FORMAT);
    auto folder = new_file.GetFolderPath();
    Utils::CreateFolder(folder);
    current_filepath_ = new_file.GetFullPath();

    if (config.record_type != RecordType::Snapshot) {
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

        header_written_ = false;

        av_dump_format(fmt_ctx, 0, new_file.GetFullPath().c_str(), 1);
    }

    if (video_recorder) {
        video_recorder->Start();
    }
    if (audio_recorder) {
        audio_recorder->Start();
    }

    if (config.record_type != RecordType::Video) {
        auto image_path = ReplaceExtension(new_file.GetFullPath(), PREVIEW_IMAGE_EXTENSION);
        MakePreviewImage(image_path);
    }

    has_first_keyframe = true;
    elapsed_time_ = 0.0;
    time_reset_pending_ = true;
}

void RecorderManager::Stop() {
    has_first_keyframe = false;
    current_filepath_.clear();

    if (video_recorder) {
        video_recorder->Stop();
    }
    if (audio_recorder) {
        audio_recorder->Stop();
    }

    {
        std::lock_guard<std::mutex> lock(ctx_mux);
        RecUtil::CloseContext(fmt_ctx);
        fmt_ctx = nullptr;
        header_written_ = false;
    }
}

RecorderManager::~RecorderManager() {
    printf("~RecorderManager\n");
    Stop();
    worker_.reset();
    video_recorder.reset();
    audio_recorder.reset();
}

std::string RecorderManager::current_filepath() const { return current_filepath_; }

bool RecorderManager::is_recording() const { return has_first_keyframe.load(); }

void RecorderManager::MakePreviewImage(std::string path) {
    // Capture video_src_ by value (shared_ptr copy) so the thread holds its own
    // reference, preventing use-after-free if RecorderManager is destroyed
    // before the 3-second delay completes.
    auto video_src = video_src_;
    auto record_stream_idx = config.record_stream_idx;
    auto jpeg_quality = config.jpeg_quality;
    std::thread([video_src, path, record_stream_idx, jpeg_quality]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (!video_src) {
            return;
        }
        auto i420buff = video_src->GetI420Frame(record_stream_idx);
        Utils::CreateJpegImage(i420buff->DataY(), i420buff->width(), i420buff->height(), path,
                               jpeg_quality);
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
