#ifndef LIBARGUS_EGL_CAPTURER_H_
#define LIBARGUS_EGL_CAPTURER_H_

#include <vector>

#include "args.h"
#include "capturer/video_capturer.h"
#include "common/logging.h"
#include "common/worker.h"

// Jetson Multimedia API
#include <Argus/Argus.h>
#include <Argus/Ext/DolWdrSensorMode.h>
#include <Argus/Ext/PwlWdrSensorMode.h>
#include <Argus/Stream.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>
#include <nvbufsurface.h>

class StreamHandler : public Subject<V4L2FrameBufferRef> {
  public:
    static std::unique_ptr<StreamHandler> Create(int stream_idx, Argus::Size2D<uint32_t> size) {
        auto ptr = std::make_unique<StreamHandler>(stream_idx, size);

        return ptr;
    }

    StreamHandler(int stream_idx, Argus::Size2D<uint32_t> size)
        : stream_idx_(stream_idx),
          size_(size),
          running_(true) {}

    ~StreamHandler() {
        running_ = false;
        worker_.reset();
        DestroyNvBufferFromFd();
    }

    uint32_t width() const { return size_.width(); }
    uint32_t height() const { return size_.height(); }
    Argus::Size2D<uint32_t> GetSize() const { return size_; }

    void SetFrameBuffer(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
        frame_buffer_ = frame_buffer;
    }
    rtc::scoped_refptr<V4L2FrameBuffer> GetFrameBuffer() { return frame_buffer_; }
    void SetOutputStream(Argus::OutputStream *stream) { output_stream_ = stream; }
    void StartCapture();

  private:
    int stream_idx_;
    int dma_fd_ = -1;
    std::atomic<bool> running_;
    Argus::Size2D<uint32_t> size_;

    Argus::OutputStream *output_stream_ = nullptr;
    Argus::UniqueObj<EGLStream::FrameConsumer> consumer_;
    EGLStream::IFrameConsumer *i_consumer_ = nullptr;

    std::unique_ptr<Worker> worker_;
    rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer_;

    void CaptureImage();
    void DestroyNvBufferFromFd();
};

class LibargusEglCapturer : public VideoCapturer {
  public:
    static std::shared_ptr<LibargusEglCapturer> Create(Args args);

    LibargusEglCapturer(Args args);
    ~LibargusEglCapturer();
    int fps() const override;
    int width(int stream_idx = 0) const override;
    int height(int stream_idx = 0) const override;
    bool has_sub_stream() const override { return num_streams_ > 1; }
    bool is_dma_capture() const override;
    uint32_t format() const override;
    Args config() const override;

    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame(int stream_idx = 0) override;
    void StartCapture() override;

    // Sub stream for AI processing
    Subscription Subscribe(Subject<V4L2FrameBufferRef>::Callback callback,
                           int stream_idx = 0) override;

    void PrintSensorModeInfo(Argus::SensorMode *mode, const char *indent);
    void PrintCameraDeviceInfo(Argus::CameraDevice *device, const char *indent);

  private:
    int camera_id_;
    int num_streams_;
    int fps_;
    uint32_t format_;
    Args config_;

    Argus::CameraDevice *camera_device_;
    Argus::UniqueObj<Argus::CameraProvider> camera_provider_;
    Argus::UniqueObj<Argus::CaptureSession> capture_session_;
    Argus::ICaptureSession *icapture_session_;
    Argus::UniqueObj<Argus::Request> request_;
    std::vector<Argus::UniqueObj<Argus::OutputStream>> output_streams_;

    std::vector<std::unique_ptr<StreamHandler>> stream_handlers_;

    void Initialize();
    void InitStreams();

    Argus::SensorMode *FindBestSensorMode(int req_width, int req_height, int req_fps);
};

#endif
