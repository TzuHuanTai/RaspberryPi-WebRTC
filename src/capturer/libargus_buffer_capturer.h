#ifndef LIBARGUS_BUFFER_CAPTURER_H_
#define LIBARGUS_BUFFER_CAPTURER_H_

#include <vector>

#include "args.h"
#include "capturer/video_capturer.h"
#include "common/worker.h"

// Jetson Multimedia API
#include "nvmmapi/NvNativeBuffer.h"
#include <Argus/Argus.h>
#include <Argus/BufferStream.h>
#include <Argus/Stream.h>
#include <NvBufSurface.h>
#include <NvBuffer.h>

class DmaBuffer : public ArgusSamples::NvNativeBuffer,
                  public NvBuffer {
  public:
    /* Always use this static method to create DmaBuffer */
    static DmaBuffer *create(const Argus::Size2D<uint32_t> &size,
                             NvBufSurfaceColorFormat colorFormat,
                             NvBufSurfaceLayout layout = NVBUF_LAYOUT_PITCH) {
        DmaBuffer *buffer = new DmaBuffer(size);
        if (!buffer)
            return NULL;

        NvBufSurf::NvCommonAllocateParams cParams;

        cParams.memtag = NvBufSurfaceTag_CAMERA;
        cParams.width = size.width();
        cParams.height = size.height();
        cParams.colorFormat = colorFormat;
        cParams.layout = layout;
        cParams.memType = NVBUF_MEM_SURFACE_ARRAY;

        if (NvBufSurf::NvAllocate(&cParams, 1, &buffer->m_fd)) {
            delete buffer;
            return NULL;
        }

        /* save the DMABUF fd in NvBuffer structure */
        buffer->planes[0].fd = buffer->m_fd;
        /* byteused must be non-zero for a valid buffer */
        buffer->planes[0].bytesused = 1;

        return buffer;
    }

    /* Help function to convert Argus Buffer to DmaBuffer */
    static DmaBuffer *fromArgusBuffer(Argus::Buffer *buffer) {
        Argus::IBuffer *i_buffer = interface_cast<Argus::IBuffer>(buffer);
        const DmaBuffer *dmabuf = static_cast<const DmaBuffer *>(i_buffer->getClientData());

        return const_cast<DmaBuffer *>(dmabuf);
    }

    /* Return DMA buffer handle */
    int getFd() const { return m_fd; }

    /* Get timestamp*/
    timeval getTimeval() const {
        Argus::IBuffer *i_buffer = interface_cast<Argus::IBuffer>(m_buffer);
        auto metadata = i_buffer->getMetadata();
        const Argus::ICaptureMetadata *imetadata =
            Argus::interface_cast<const Argus::ICaptureMetadata>(metadata);

        timeval tv = {};
        tv.tv_sec = imetadata->getSensorTimestamp() / 1000000000;
        tv.tv_usec = (imetadata->getSensorTimestamp() % 1000000000) / 1000;
        return tv;
    }

    /* Get and set reference to Argus buffer */
    void setArgusBuffer(Argus::Buffer *buffer) { m_buffer = buffer; }
    Argus::Buffer *getArgusBuffer() const { return m_buffer; }

  private:
    DmaBuffer(const Argus::Size2D<uint32_t> &size)
        : NvNativeBuffer(size),
          NvBuffer(0, 0),
          m_buffer(NULL) {}

    Argus::Buffer *m_buffer; /* Reference to Argus::Buffer */
};

class LibargusBufferCapturer : public VideoCapturer {
  public:
    static std::shared_ptr<LibargusBufferCapturer> Create(Args args);

    LibargusBufferCapturer(Args args);
    ~LibargusBufferCapturer();
    int fps() const override;
    int width() const override;
    int height() const override;
    bool is_dma_capture() const override;
    uint32_t format() const override;
    Args config() const override;

    void Initialize();
    void StartCapture() override;
    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame() override;

    void PrintSensorModeInfo(Argus::SensorMode *mode, const char *indent);
    void PrintCameraDeviceInfo(Argus::CameraDevice *device, const char *indent);

  private:
    int camera_id_;
    int fps_;
    int width_;
    int height_;
    int framesize_;
    int stride_;
    int buffer_count_; /* This value is tricky. Too small value will impact the FPS */
    uint32_t format_;
    bool abort_;
    Args config_;
    std::unique_ptr<Worker> consumer_;
    std::unique_ptr<Worker> producer_;

    rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer_;

    std::vector<NvBufSurface *> surf_;
    std::vector<DmaBuffer *> native_buffers_;
    Argus::Size2D<uint32_t> stream_size_;
    Argus::UniqueObj<Argus::OutputStream> output_stream_;
    Argus::UniqueObj<Argus::EventQueue> ev_queue_;
    Argus::IEventQueue *iqueue_;
    Argus::IEventProvider *ievent_provider_;
    Argus::ICaptureSession *icapture_session_;
    Argus::IBufferOutputStream *ibuffer_output_stream_;

    void RunProducer();
    void RunConsumer();
    void CaptureImage();
    Argus::SensorMode *FindBestSensorMode(Argus::CameraDevice *camera_device, int req_width,
                                          int req_height, int req_fps);
    void PrintErrorStatus(Argus::Status status);
};

#endif
