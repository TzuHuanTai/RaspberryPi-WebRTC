#include "args.h"
#include "capturer/v4l2_capturer.h"
#include "v4l2_codecs/v4l2_decoder.h"

#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

void WriteYuvImage(void *start, int length, int index) {
    std::string filename = "img" + std::to_string(index) + ".yuv";
    FILE *file = fopen(filename.c_str(), "wb");
    if (file) {
        fwrite((uint8_t *)start, 1, length, file);
        fclose(file);
    } else {
        fprintf(stderr, "Failed to open file for writing: %s\n", filename.c_str());
    }
}

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int images_nb = 0;
    int record_sec = 1;
    Args args{.fps = 15,
              .width = 640,
              .height = 480,
              .hw_accel = false,
              .format = V4L2_PIX_FMT_MJPEG,
              .device = "/dev/video0"};

    auto capturer = V4l2Capturer::Create(args);
    auto decoder = V4l2Decoder::Create(args.width, args.height, capturer->format(), false);

    auto observer = capturer->AsRawBufferObservable();
    observer->Subscribe([&](V4l2Buffer buffer) {
        printf("Camera buffer: %u\n", buffer.length);
        decoder->EmplaceBuffer(buffer, [&](V4l2Buffer decoded_buffer) {
            if (is_finished) {
                return;
            }

            if (images_nb++ < args.fps * record_sec) {
                printf("Buffer index: %d\n  bytesused: %u\n", images_nb, decoded_buffer.length);
                WriteYuvImage(decoded_buffer.start, decoded_buffer.length, images_nb);
            } else {
                is_finished = true;
                cond_var.notify_all();
            }
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] {
        return is_finished;
    });

    decoder.reset();
    observer->UnSubscribe();

    return 0;
}
