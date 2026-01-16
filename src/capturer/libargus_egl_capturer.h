#ifndef LIBARGUS_EGL_CAPTURER_H_
#define LIBARGUS_EGL_CAPTURER_H_

#include <vector>

#include "args.h"
#include "capturer/video_capturer.h"
#include "common/worker.h"

// Jetson Multimedia API
#include <Argus/Argus.h>
#include <Argus/Ext/DolWdrSensorMode.h>
#include <Argus/Ext/PwlWdrSensorMode.h>
#include <Argus/Stream.h>
#include <EGLStream/EGLStream.h>
#include <EGLStream/NV/ImageNativeBuffer.h>
#include <nvbufsurface.h>

class LibargusEglCapturer : public VideoCapturer {
  public:
    static std::shared_ptr<LibargusEglCapturer> Create(Args args);

    LibargusEglCapturer(Args args);
    ~LibargusEglCapturer();
    int fps() const override;
    int width(int stream_idx = 0) const override;
    int height(int stream_idx = 0) const override;
    bool is_dma_capture() const override;
    uint32_t format() const override;
    Args config() const override;

    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame(int stream_idx = 0) override;
    void StartCapture() override;

    // Sub stream for AI processing
    Subscription SubscribeSub(Subject<V4L2FrameBufferRef>::Callback callback) override;
    bool has_sub_stream() const override { return num_streams_ > 1; }

    void PrintSensorModeInfo(Argus::SensorMode *mode, const char *indent);
    void PrintCameraDeviceInfo(Argus::CameraDevice *device, const char *indent);

  private:
    int camera_id_;
    int num_streams_;
    int fps_;
    std::vector<int> dma_fds_;
    std::vector<int> widths_;
    std::vector<int> heights_;
    std::vector<int> framesizes_;
    uint32_t format_;
    Args config_;
    std::mutex control_mutex_;
    std::vector<std::unique_ptr<Worker>> workers_;
    std::vector<rtc::scoped_refptr<V4L2FrameBuffer>> frame_buffer_;

    Argus::CameraDevice *camera_device_;
    Argus::UniqueObj<Argus::Request> request_;
    std::vector<Argus::UniqueObj<Argus::OutputStream>> output_streams_;
    Argus::UniqueObj<Argus::CameraProvider> camera_provider_;
    std::vector<Argus::UniqueObj<EGLStream::FrameConsumer>> frame_consumers_;
    Argus::IRequest *irequest_;
    Argus::ICaptureSession *icapture_session_;
    Argus::ICameraProvider *icamera_provider_;
    std::vector<EGLStream::IFrameConsumer *> iframe_consumers_;
    std::vector<Argus::IEGLOutputStream *> iegl_streams_;
    Argus::IEGLOutputStreamSettings *istream_settings_;

    // Sub stream resources
    Subject<V4L2FrameBufferRef> sub_stream_;

    void Initialize();
    void InitStreams();
    void CaptureImage(int stream_idx);
    void DestroyNvBufferFromFd(int &fd);
    Argus::SensorMode *FindBestSensorMode(int req_width, int req_height, int req_fps);
};

#endif
