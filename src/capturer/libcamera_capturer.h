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
    ~LibcameraCapturer() override;

    int fps() const override;
    int width(int stream_idx = 0) const override;
    int height(int stream_idx = 0) const override;
    bool is_dma_capture() const override;
    uint32_t format() const override;
    Args config() const override;

    bool SetControls(int key, int value) override;
    void StartCapture() override;

    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame(int stream_idx = 0) override;
    Subscription Subscribe(Subject<V4L2FrameBufferRef>::Callback callback,
                           int stream_idx = 0) override;

  private:
    int camera_id_;
    int fps_;
    int width_;
    int height_;
    int stride_;
    int rotation_;
    int buffer_count_;
    uint32_t format_;
    Args config_;
    std::mutex control_mutex_;
    std::atomic<bool> is_controls_updated_;

    std::unique_ptr<libcamera::CameraManager> cm_;
    std::shared_ptr<libcamera::Camera> camera_;
    std::unique_ptr<libcamera::CameraConfiguration> camera_config_;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
    std::vector<std::unique_ptr<libcamera::Request>> requests_;
    libcamera::Stream *stream_;
    libcamera::ControlList controls_;
    std::map<int, std::pair<void *, unsigned int>> mapped_buffers_;

    V4L2FrameBufferRef frame_buffer_;
    Subject<V4L2FrameBufferRef> stream_subject_;

    void InitCamera();
    void InitControls(Args arg);
    void AllocateBuffer();
    void RequestComplete(libcamera::Request *request);
};

#endif
