#ifndef LIBCAMERA_CAPTURER_H_
#define LIBCAMERA_CAPTURER_H_

#include <vector>

#include <libcamera/libcamera.h>
#include <modules/video_capture/video_capture.h>

#include "args.h"
#include "capturer/video_capturer.h"
#include "common/interface/subject.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"

class LibcameraCapturer : public VideoCapturer {
  public:
    static std::shared_ptr<LibcameraCapturer> Create(Args args);

    LibcameraCapturer(Args args);
    ~LibcameraCapturer();
    int fps() const override;
    int width() const override;
    int height() const override;
    bool is_dma_capture() const override;
    uint32_t format() const override;
    Args config() const override;

    LibcameraCapturer &SetControls(const int id, const int value) override;
    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame() override;
    void StartCapture() override;

  private:
    int fps_;
    int width_;
    int height_;
    int stride_;
    int buffer_count_;
    uint32_t format_;
    Args config_;
    std::mutex control_mutex_;

    std::unique_ptr<libcamera::CameraManager> cm_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> camera_config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    libcamera::Stream *stream_;
    libcamera::ControlList controls_;
    std::map<int, std::pair<void *, unsigned int>> mappedBuffers_;

    rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer_;
    void NextBuffer(V4l2Buffer &raw_buffer);

    LibcameraCapturer &SetFormat(int width, int height);
    LibcameraCapturer &SetFps(int fps);
    LibcameraCapturer &SetRotation(int angle);

    void Init(std::string device);
    void AllocateBuffer();
    void RequestComplete(libcamera::Request *request);
};

#endif
