#include "capturer/v4l2_capturer.h"

#include <condition_variable>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <unistd.h>

void WriteImage(V4L2Buffer buffer, int index) {
    printf("Dequeue buffer index: %d\n"
           "  bytesused: %d\n",
           index, buffer.length);

    std::string filename = "img" + std::to_string(index) + ".jpg";
    int outfd = open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno)) {
        /* open the existing file with write flag */
        outfd = open(filename.c_str(), O_WRONLY);
    }

    write(outfd, buffer.start, buffer.length);
}

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int i = 0;
    int images_nb = 10;
    Args args{.fps = 15, .width = 1280, .height = 720, .cameraId = 0, .format = V4L2_PIX_FMT_MJPEG};

    auto capturer = V4L2Capturer::Create(args);
    auto observer = capturer->AsRawBufferObservable();
    observer->Subscribe([&](V4L2Buffer buffer) {
        if (i < images_nb) {
            WriteImage(buffer, ++i);
        } else {
            is_finished = true;
            cond_var.notify_all();
        }
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] {
        return is_finished;
    });

    return 0;
}
