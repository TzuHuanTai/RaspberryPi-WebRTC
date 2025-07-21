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
    int width() const override;
    int height() const override;
    bool is_dma_capture() const override;
    uint32_t format() const override;
    Args config() const override;

    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame() override;
    void StartCapture() override;

    void PrintSensorModeInfo(Argus::SensorMode *mode, const char *indent);
    void PrintCameraDeviceInfo(Argus::CameraDevice *device, const char *indent);

  private:
    int cameraId_;
    int dma_fd_;
    int fps_;
    int width_;
    int height_;
    int framesize_;
    int stride_;
    uint32_t format_;
    Args config_;
    std::mutex control_mutex_;
    std::unique_ptr<Worker> worker_;

    rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer_;
    Argus::CameraDevice *camera_device_;
    Argus::UniqueObj<Argus::Request> request_;
    Argus::UniqueObj<Argus::OutputStream> output_stream_;
    Argus::UniqueObj<Argus::CameraProvider> camera_provider_;
    Argus::UniqueObj<EGLStream::FrameConsumer> frame_consumer_;
    Argus::IRequest *irequest_;
    Argus::ICaptureSession *icapture_session_;
    Argus::ICameraProvider *icamera_provider_;
    EGLStream::IFrameConsumer *iframe_consumer_;
    Argus::IEGLOutputStream *iegl_stream_;
    Argus::IEGLOutputStreamSettings *istream_settings_;

    void InitCamera();
    void CaptureImage();
    void DestroyNvBufferFromFd(int &fd);
    Argus::SensorMode *FindBestSensorMode(int req_width, int req_height, int req_fps);
};

#endif
