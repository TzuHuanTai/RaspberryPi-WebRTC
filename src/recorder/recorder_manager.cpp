#include "recorder/recorder_manager.h"

#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"
#include "recorder/utils.h"
#include "common/utils.h"

#include <csignal>
#include <filesystem>
#include <mutex>
#include <condition_variable>

const double SECOND_PER_FILE = 60.0;

std::unique_ptr<RecorderManager> RecorderManager::Create(
        std::shared_ptr<Conductor> conductor,
        std::string record_path) {
    auto instance = std::make_unique<RecorderManager>(record_path);

    auto video_src = conductor->VideoSource();
    if (video_src) {
        instance->CreateVideoRecorder(video_src);
        instance->SubscribeVideoSource(video_src);
    }
    auto audio_src = conductor->AudioSource();
    if (audio_src) {
        instance->CreateAudioRecorder(audio_src);
        instance->SubscribeAudioSource(audio_src);
    }
    return instance;
}

void RecorderManager::CreateVideoRecorder(
    std::shared_ptr<V4L2Capture> capture) {
    fps = capture->fps();
    width = capture->width();
    height = capture->height();
    video_recorder = ([capture]() -> std::unique_ptr<VideoRecorder> {
        if (capture->format() == V4L2_PIX_FMT_H264) {
            return RawH264Recorder::Create(capture->config());
        } else {
            return H264Recorder::Create(capture->config());
        }
    })();
}

void RecorderManager::CreateAudioRecorder(std::shared_ptr<PaCapture> capture) {
    audio_recorder = ([capture]() -> std::unique_ptr<AudioRecorder> {
        return AudioRecorder::Create(capture->config());
    })();
}

RecorderManager::RecorderManager(std::string record_path)
    : fmt_ctx(nullptr),
      has_first_keyframe(false),
      record_path(record_path),
      elapsed_time_(0.0) {}

void RecorderManager::SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src) {
    video_observer = video_src->AsObservable();
    video_observer->Subscribe([this](V4l2Buffer buffer) {
        // waiting first keyframe to start recorders.
        if (!has_first_keyframe && (buffer.flags & V4L2_BUF_FLAG_KEYFRAME)) {
            Start();
            last_created_time_ = buffer.timestamp;
        }

        // restart to write in the new file.
        if (elapsed_time_ >= SECOND_PER_FILE && buffer.flags & V4L2_BUF_FLAG_KEYFRAME) {
            last_created_time_ = buffer.timestamp;
            Stop();
            Start();
        }

        if (has_first_keyframe && video_recorder) {
            video_recorder->OnBuffer(buffer);
            elapsed_time_ = (buffer.timestamp.tv_sec - last_created_time_.tv_sec) +
                            (buffer.timestamp.tv_usec - last_created_time_.tv_usec) / 1000000.0;
        }
    });

    video_recorder->OnPacketed([this](AVPacket *pkt) {
        this->WriteIntoFile(pkt);
    });
}

void RecorderManager::SubscribeAudioSource(std::shared_ptr<PaCapture> audio_src) {
    audio_observer = audio_src->AsObservable();
    audio_observer->Subscribe([this](PaBuffer buffer) {
        if (has_first_keyframe && audio_recorder) {
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
    if (!Utils::CheckDriveSpace(record_path, 100)) {
        printf("[RecorderManager] Skip recording since not enough free space!\n");
        return;
    }

    std::lock_guard<std::mutex> lock(ctx_mux);
    filename = Utils::GenerateFilename();
    fmt_ctx = RecUtil::CreateContainer(record_path, filename);

    if (video_recorder) {
        video_recorder->SetFilename(filename);
        video_recorder->AddStream(fmt_ctx);
        video_recorder->Start();
    }
    if (audio_recorder) {
        audio_recorder->AddStream(fmt_ctx);
        audio_recorder->Start();
    }
    RecUtil::WriteFormatHeader(fmt_ctx);

    has_first_keyframe = true;
}

void RecorderManager::Stop() {
    std::lock_guard<std::mutex> lock(ctx_mux);
    if (video_recorder) {
        video_recorder->Stop();
    }
    if (audio_recorder) {
        audio_recorder->Stop();
    }

    if (fmt_ctx) {
        RecUtil::CloseContext(fmt_ctx);
        fmt_ctx = nullptr;
    }
}

RecorderManager::~RecorderManager() {
    video_recorder.reset();
    audio_recorder.reset();
    video_observer->UnSubscribe();
    audio_observer->UnSubscribe();
    Stop();    
}
