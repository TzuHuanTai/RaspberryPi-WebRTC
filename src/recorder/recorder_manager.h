#ifndef RECORDER_MANAGER_H_
#define RECORDER_MANAGER_H_

#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswscale/swscale.h>
}

#include "capturer/pa_capturer.h"
#include "capturer/video_capturer.h"
#include "common/worker.h"
#include "recorder/audio_recorder.h"
#include "recorder/video_recorder.h"

class RecUtil {
  public:
    static AVFormatContext *CreateContainer(const std::string &full_path);
    static bool WriteFormatHeader(AVFormatContext *fmt_ctx);
    static void CloseContext(AVFormatContext *fmt_ctx);
};

class RecorderManager {
  public:
    static std::unique_ptr<RecorderManager> Create(std::shared_ptr<VideoCapturer> video_src,
                                                   std::shared_ptr<PaCapturer> audio_src,
                                                   Args config);
    RecorderManager(Args config);
    ~RecorderManager();
    void WriteIntoFile(AVPacket *pkt);
    void Start();
    void Stop();

  protected:
    std::mutex ctx_mux;
    Args config;
    uint fps;
    int width;
    int height;
    std::string record_path;
    AVFormatContext *fmt_ctx;
    bool has_first_keyframe;
    std::shared_ptr<Observable<rtc::scoped_refptr<V4L2FrameBuffer>>> video_observer;
    std::shared_ptr<Observable<PaBuffer>> audio_observer;
    std::unique_ptr<VideoRecorder> video_recorder;
    std::unique_ptr<AudioRecorder> audio_recorder;

    void CreateVideoRecorder(std::shared_ptr<VideoCapturer> video_src);
    void CreateAudioRecorder(std::shared_ptr<PaCapturer> aduio_src);
    void SubscribeVideoSource(std::shared_ptr<VideoCapturer> video_src);
    void SubscribeAudioSource(std::shared_ptr<PaCapturer> aduio_src);

  private:
    double elapsed_time_;
    std::mutex rotation_mtx_;
    std::unique_ptr<Worker> worker_;
    struct timeval last_created_time_;
    std::shared_ptr<VideoCapturer> video_src_;

    void MakePreviewImage();
    std::string ReplaceExtension(const std::string &url, const std::string &new_extension);
};

#endif // RECORDER_MANAGER_H_
