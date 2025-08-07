#include "args.h"
#include "capturer/libargus_buffer_capturer.h"
#include "codecs/jetson/jetson_encoder.h"
#include "codecs/jetson/jetson_scaler.h"

#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    bool has_first_keyframe_ = false;
    int images_nb = 0;
    int record_sec = 100;
    int adapted_width = 1280;
    int adapted_height = 720;
    Args args{
        .cameraId = 0,
        .fps = 30,
        .width = 1920,
        .height = 1080,
        .hw_accel = true, // hw will use dma fd
    };

    auto capturer = LibargusBufferCapturer::Create(args);
    auto observer = capturer->AsFrameBufferObservable();
    auto scaler = JetsonScaler::Create(args.width, args.height, adapted_width, adapted_height);
    auto encoder =
        JetsonEncoder::Create(adapted_width, adapted_height, V4L2_PIX_FMT_H264, args.hw_accel);

    int cam_frame_count = 0;
    auto cam_start_time = std::chrono::steady_clock::now();
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    observer->Subscribe([&](rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
        auto buffer = frame_buffer->GetRawBuffer();

        auto cam_current_time = std::chrono::steady_clock::now();
        cam_frame_count++;

        if (std::chrono::duration_cast<std::chrono::seconds>(cam_current_time - cam_start_time)
                .count() >= 1) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(cam_current_time -
                                                                                  cam_start_time)
                                .count();
            double cam_current_fps = static_cast<double>(cam_frame_count) / (duration / 1000.0);
            cam_start_time = cam_current_time;
            printf("Camera FPS: %.2f\n", cam_current_fps);
            cam_frame_count = 0;
        }

        auto time_to_start =
            std::chrono::duration_cast<std::chrono::milliseconds>(cam_current_time - start_time)
                .count();
        printf("Camera time: %ld ms, camera fd: %d\n", time_to_start, frame_buffer->GetDmaFd());

        scaler->EmplaceBuffer(frame_buffer, [&](V4L2FrameBufferRef scaled_buffer) {
            auto current_time = std::chrono::steady_clock::now();
            auto latency_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time)
                    .count();

            int scaled_fd = scaled_buffer->GetDmaFd();
            printf("Scaler time: %ld ms, src fd(%d) to dst fd(%d)\n", latency_ms,
                   frame_buffer->GetDmaFd(), scaled_fd);

            encoder->EmplaceBuffer(scaled_buffer, [&,
                                                   scaled_fd](V4L2FrameBufferRef encoded_buffer) {
                if (is_finished) {
                    return;
                }

                auto current_time = std::chrono::steady_clock::now();
                frame_count++;

                auto latency_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time)
                        .count();
                printf("Encoder time: %ld ms, src fd(%d) to dst fd(%d)\n", latency_ms, scaled_fd,
                       encoded_buffer->GetDmaFd());

                if (std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time)
                        .count() >= 1) {
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        current_time - start_time)
                                        .count();
                    double current_fps = static_cast<double>(frame_count) / (duration / 1000.0);
                    start_time = current_time;
                    printf("Scaler FPS: %.2f\n", current_fps);
                    frame_count = 0;
                }

                if (images_nb++ > args.fps * record_sec) {
                    is_finished = true;
                    cond_var.notify_all();
                }
            });
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] {
        return is_finished;
    });

    // encoder.reset();
    observer->UnSubscribe();

    return 0;
}
