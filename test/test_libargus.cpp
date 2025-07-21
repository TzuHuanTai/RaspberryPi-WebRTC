#include "args.h"
#include "capturer/libargus_buffer_capturer.h"

#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

using namespace Argus;

void WriteImage(void *start, int length, int index) {
    printf("Dequeued buffer index: %d\n"
           "  bytesused: %d\n",
           index, length);

    std::string filename = "img" + std::to_string(index) + ".yuv";
    int outfd = open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno)) {
        /* open the existing file with write flag */
        outfd = open(filename.c_str(), O_WRONLY);
    }

    write(outfd, start, length);
}

int main() {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int frame_count = 0;
    int record_sec = 1000;
    Args args{.fps = 60, .width = 1280, .height = 720};
    auto start_time = std::chrono::steady_clock::now();

    auto capturer = LibargusBufferCapturer::Create(args);

    auto observer = capturer->AsFrameBufferObservable();
    observer->Subscribe([&](rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
        if (is_finished) {
            return;
        }

        auto current_time = std::chrono::steady_clock::now();
        frame_count++;

        if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() >=
            1) {
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time)
                    .count();
            double current_fps = static_cast<double>(frame_count) / (duration / 1000.0);
            start_time = current_time;
            printf("Codec FPS: %.2f\n", current_fps);
            frame_count = 0;
        }

        if (frame_count > args.fps * record_sec) {
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
