#include "recorder/recorder_manager.h"

#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <mutex>

#include "common/logging.h"
#include "common/utils.h"
#include "common/v4l2_frame_buffer.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"

const double SECOND_PER_FILE = 60.0;

AVFormatContext *RecUtil::CreateContainer(std::string record_path, std::string filename) {
    AVFormatContext *fmt_ctx = nullptr;
    std::string container = "mp4";
    auto full_path = record_path + '/' + filename + "." + container;

    if (avformat_alloc_output_context2(&fmt_ctx, nullptr, container.c_str(), full_path.c_str()) <
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
    av_dump_format(fmt_ctx, 0, full_path.c_str(), 1);

    return fmt_ctx;
}

void RecUtil::CreateThumbnail(std::string record_path, std::string filename) {
    const std::string ffmpegCommand =
        std::string("ffmpeg -xerror -loglevel quiet -hide_banner -y") + " -i " + record_path + "/" +
        filename + ".mp4" + " -vf \"select=eq(pict_type\\,I)\" -vsync vfr -frames:v 1 " +
        record_path + "/" + filename + ".jpg";
    DEBUG_PRINT("%s", ffmpegCommand.c_str());

    // Execute the command
    int result = std::system(ffmpegCommand.c_str());

    // Check the result
    if (result == 0) {
        DEBUG_PRINT("Thumbnail created successfully.");
    } else {
        DEBUG_PRINT("Error executing FFmpeg command.");
    }
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

std::unique_ptr<RecorderManager> RecorderManager::Create(std::shared_ptr<Conductor> conductor,
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

    instance->StartRotationThread();

    return instance;
}

void RecorderManager::CreateVideoRecorder(std::shared_ptr<V4L2Capture> capture) {
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

void RecorderManager::StartRotationThread() {
    rotation_worker_.reset(new Worker("Record Rotation", [this]() {
        while (!Utils::CheckDriveSpace(record_path, 400)) {
            Utils::RotateFiles(record_path);
        }
        sleep(60);
    }));
    rotation_worker_->Run();
}

void RecorderManager::SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src) {
    video_observer = video_src->AsObservable();
    video_observer->Subscribe([this](rtc::scoped_refptr<V4l2FrameBuffer> buffer) {
        // waiting first keyframe to start recorders.
        if (!has_first_keyframe && (buffer->flags() & V4L2_BUF_FLAG_KEYFRAME)) {
            Start();
            last_created_time_ = buffer->timestamp();
        }

        // restart to write in the new file.
        if (elapsed_time_ >= SECOND_PER_FILE && buffer->flags() & V4L2_BUF_FLAG_KEYFRAME) {
            last_created_time_ = buffer->timestamp();
            Stop();
            Start();
        }

        if (has_first_keyframe && video_recorder) {
            video_recorder->OnBuffer(buffer);
            elapsed_time_ = (buffer->timestamp().tv_sec - last_created_time_.tv_sec) +
                            (buffer->timestamp().tv_usec - last_created_time_.tv_usec) / 1000000.0;
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
        DEBUG_PRINT("Skip recording since not enough free space!");
        return;
    }

    std::lock_guard<std::mutex> lock(ctx_mux);
    auto file_info = Utils::GenerateFilename();
    auto folder = record_path + file_info.date + "/" + file_info.hour;
    Utils::CreateFolder(folder);
    fmt_ctx = RecUtil::CreateContainer(folder, file_info.filename);

    if (video_recorder) {
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
    Stop();
    video_recorder.reset();
    audio_recorder.reset();
    video_observer->UnSubscribe();
    audio_observer->UnSubscribe();
    rotation_worker_.reset();
}
