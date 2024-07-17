#ifndef RECODER_MANAGER_H_
#define RECODER_MANAGER_H_

#include "capture/v4l2_capture.h"
#include "capture/pa_capture.h"
#include "conductor.h"
#include "recorder/audio_recorder.h"
#include "recorder/video_recorder.h"
#include "common/worker.h"

#include <mutex>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswscale/swscale.h>
}

class RecUtil {
public:
    static AVFormatContext* CreateContainer(std::string record_path, std::string filename);
    static void CreateThumbnail(std::string record_path, std::string filename);
    static bool WriteFormatHeader(AVFormatContext* fmt_ctx);
    static void CloseContext(AVFormatContext* fmt_ctx);
};

class RecorderManager {
public:
    static std::unique_ptr<RecorderManager> Create(
        std::shared_ptr<Conductor> conductor, 
        std::string record_path);
    RecorderManager(std::string record_path);
    ~RecorderManager();
    void WriteIntoFile(AVPacket *pkt);
    void Start();
    void Stop();

protected:
    std::mutex ctx_mux;
    uint fps;
    int width;
    int height;
    std::string record_path;
    AVFormatContext *fmt_ctx;
    bool has_first_keyframe;
    std::shared_ptr<Observable<rtc::scoped_refptr<V4l2FrameBuffer>>> video_observer;
    std::shared_ptr<Observable<PaBuffer>> audio_observer;
    std::unique_ptr<VideoRecorder> video_recorder;
    std::unique_ptr<AudioRecorder> audio_recorder;

    void CreateVideoRecorder(std::shared_ptr<V4L2Capture> video_src);
    void CreateAudioRecorder(std::shared_ptr<PaCapture> aduio_src);
    void SubscribeVideoSource(std::shared_ptr<V4L2Capture> video_src);
    void SubscribeAudioSource(std::shared_ptr<PaCapture> aduio_src);

private:
    double elapsed_time_;
    struct timeval last_created_time_;
    std::unique_ptr<Worker> rotation_worker_;

    void StartRotationThread();
};

#endif
