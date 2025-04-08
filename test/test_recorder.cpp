#include "capturer/libcamera_capturer.h"
#include "recorder/recorder_manager.h"

int main(int argc, char *argv[]) {
    Args args{.fps = 15,
              .width = 1280,
              .height = 720,
              .sample_rate = 48000,
              .format = V4L2_PIX_FMT_YUV420,
              .record_path = "./"};

    auto video_capture = LibcameraCapturer::Create(args);
    auto audio_capture = PaCapturer::Create(args);
    auto recorder_mgr = RecorderManager::Create(video_capture, audio_capture, args);
    sleep(5);

    return 0;
}
