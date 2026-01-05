#include "libargus_buffer_capturer.h"

#include <iostream>
#include <sys/mman.h>

#include "common/logging.h"

static const uint64_t WAIT_FOR_EVENT_TIMEOUT = 3'000'000'000;
static constexpr uint64_t ACQUIRE_FRAME_TIMEOUT = 3'000'000'000;

std::shared_ptr<LibargusBufferCapturer> LibargusBufferCapturer::Create(Args args) {
    auto ptr = std::make_shared<LibargusBufferCapturer>(args);
    ptr->StartCapture();
    return ptr;
}

LibargusBufferCapturer::LibargusBufferCapturer(Args args)
    : camera_id_(args.camera_id),
      fps_(args.fps),
      width_(args.width),
      height_(args.height),
      buffer_count_(8),
      format_(V4L2_PIX_FMT_NV12),
      abort_(false),
      config_(args),
      surf_(buffer_count_, nullptr),
      native_buffers_(buffer_count_, nullptr),
      stream_size_(args.width, args.height) {}

LibargusBufferCapturer::~LibargusBufferCapturer() {
    INFO_PRINT("~LibargusBufferCapturer");

    abort_ = true;

    /* Stop the repeating request and wait for idle */
    icapture_session_->stopRepeat();
    ibuffer_output_stream_->endOfStream();
    icapture_session_->waitForIdle();

    /* Destroy the output stream to end the consumer thread */
    output_stream_.reset();

    /* Destroy the EGLImages */
    for (uint32_t i = 0; i < buffer_count_; i++) {
        NvBufSurfaceUnMapEglImage(surf_[i], 0);
    }

    /* Destroy the native buffers */
    for (uint32_t i = 0; i < buffer_count_; i++) {
        delete native_buffers_[i];
    }

    producer_.reset();
    consumer_.reset();
}

int LibargusBufferCapturer::fps() const { return fps_; }

int LibargusBufferCapturer::width() const { return width_; }

int LibargusBufferCapturer::height() const { return height_; }

bool LibargusBufferCapturer::is_dma_capture() const { return config_.hw_accel; }

uint32_t LibargusBufferCapturer::format() const { return format_; }

Args LibargusBufferCapturer::config() const { return config_; }

rtc::scoped_refptr<webrtc::I420BufferInterface> LibargusBufferCapturer::GetI420Frame() {
    return frame_buffer_->ToI420();
}

void LibargusBufferCapturer::StartCapture() {
    framesize_ = width_ * height_ + ((width_ + 1) / 2) * ((height_ + 1) / 2) * 2;
    frame_buffer_ = V4L2FrameBuffer::Create(width_, height_, framesize_, format_);

    producer_ = std::make_unique<Worker>("LibargusProducer", [this]() {
        RunProducer();
    });
    producer_->Run();
}

void LibargusBufferCapturer::RunProducer() {
    /* Get default EGL display */
    EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) {
        ERROR_PRINT("Cannot get EGL display.");
        return;
    }

    /* Create the CameraProvider object and get the core interface */
    Argus::UniqueObj<Argus::CameraProvider> camera_provider =
        Argus::UniqueObj<Argus::CameraProvider>(Argus::CameraProvider::create());
    Argus::ICameraProvider *icamera_provider =
        Argus::interface_cast<Argus::ICameraProvider>(camera_provider);
    if (!icamera_provider)
        ERROR_PRINT("Failed to create CameraProvider");

    /* Get the camera devices */
    std::vector<Argus::CameraDevice *> camera_devices;
    icamera_provider->getCameraDevices(&camera_devices);
    if (camera_devices.size() == 0)
        ERROR_PRINT("No cameras available");

    if (camera_id_ >= camera_devices.size()) {
        ERROR_PRINT("CAMERA_INDEX out of range. Fall back to 0");
        camera_id_ = 0;
    }

    /* Create the capture session using the first device and get the core interface */
    Argus::UniqueObj<Argus::CaptureSession> capture_session(
        icamera_provider->createCaptureSession(camera_devices[camera_id_]));
    icapture_session_ = interface_cast<Argus::ICaptureSession>(capture_session);
    if (!icapture_session_)
        ERROR_PRINT("Failed to get ICaptureSession interface");
    ievent_provider_ = interface_cast<Argus::IEventProvider>(capture_session);

    /* Create the OutputStream */
    INFO_PRINT("Creating output stream");
    Argus::UniqueObj<Argus::OutputStreamSettings> stream_settings(
        icapture_session_->createOutputStreamSettings(Argus::STREAM_TYPE_BUFFER));
    Argus::IBufferOutputStreamSettings *istream_settings =
        interface_cast<Argus::IBufferOutputStreamSettings>(stream_settings);
    if (!istream_settings)
        ERROR_PRINT("Failed to get IBufferOutputStreamSettings interface");

    /* Configure the OutputStream to use the EGLImage BufferType */
    istream_settings->setBufferType(Argus::BUFFER_TYPE_EGL_IMAGE);

    /* Create the OutputStream */
    Argus::UniqueObj<Argus::OutputStream> output_stream_(
        icapture_session_->createOutputStream(stream_settings.get()));
    ibuffer_output_stream_ = interface_cast<Argus::IBufferOutputStream>(output_stream_);

    /* Allocate native buffers */

    for (uint32_t i = 0; i < buffer_count_; i++) {
        native_buffers_[i] =
            DmaBuffer::create(stream_size_, NVBUF_COLOR_FORMAT_NV12, NVBUF_LAYOUT_PITCH);
        if (!native_buffers_[i]) {
            ERROR_PRINT("Failed to allocate NativeBuffer");
        }
    }

    /* Create EGLImages from the native buffers */
    EGLImageKHR egl_images[buffer_count_];
    for (uint32_t i = 0; i < buffer_count_; i++) {
        int ret = 0;
        ret = NvBufSurfaceFromFd(native_buffers_[i]->getFd(), (void **)(&surf_[i]));
        if (ret) {
            ERROR_PRINT("NvBufSurfaceFromFd failed");
        }

        ret = NvBufSurfaceMapEglImage(surf_[i], 0);
        if (ret) {
            ERROR_PRINT("NvBufSurfaceMapEglImage failed");
        }

        egl_images[i] = surf_[i]->surfaceList[0].mappedAddr.eglImage;
        if (egl_images[i] == EGL_NO_IMAGE_KHR) {
            ERROR_PRINT("Failed to create EGLImage");
        }
    }

    /* Create the BufferSettings object to configure Buffer creation */
    Argus::UniqueObj<Argus::BufferSettings> buffer_settings(
        ibuffer_output_stream_->createBufferSettings());
    Argus::IEGLImageBufferSettings *ibuffer_settings =
        interface_cast<Argus::IEGLImageBufferSettings>(buffer_settings);
    if (!ibuffer_settings) {
        ERROR_PRINT("Failed to create BufferSettings");
    }

    /* Create the Buffers for each EGLImage (and release to
       stream for initial capture use) */
    Argus::UniqueObj<Argus::Buffer> buffers[buffer_count_];
    for (uint32_t i = 0; i < buffer_count_; i++) {
        ibuffer_settings->setEGLImage(egl_images[i]);
        ibuffer_settings->setEGLDisplay(egl_display);
        buffers[i].reset(ibuffer_output_stream_->createBuffer(buffer_settings.get()));
        Argus::IBuffer *ibuffer = interface_cast<Argus::IBuffer>(buffers[i]);

        /* Reference Argus::Buffer and DmaBuffer each other */
        ibuffer->setClientData(native_buffers_[i]);
        native_buffers_[i]->setArgusBuffer(buffers[i].get());

        if (!interface_cast<Argus::IEGLImageBuffer>(buffers[i]))
            ERROR_PRINT("Failed to create Buffer");
        if (ibuffer_output_stream_->releaseBuffer(buffers[i].get()) != Argus::STATUS_OK)
            ERROR_PRINT("Failed to release Buffer for capture use");
    }

    /* Launch the FrameConsumer thread to consume frames from the OutputStream */
    RunConsumer();

    /* Create capture request and enable output stream */
    Argus::UniqueObj<Argus::Request> request(icapture_session_->createRequest());
    Argus::IRequest *iRequest = interface_cast<Argus::IRequest>(request);
    if (!iRequest)
        ERROR_PRINT("Failed to create Request");
    iRequest->enableOutputStream(output_stream_.get());

    Argus::ISourceSettings *iSourceSettings =
        interface_cast<Argus::ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ERROR_PRINT("Failed to get ISourceSettings interface");
    iSourceSettings->setFrameDurationRange(Argus::Range<uint64_t>(1e9 / fps_));
    auto mode = FindBestSensorMode(camera_devices[camera_id_], width_, height_, fps_);
    iSourceSettings->setSensorMode(mode);

    /* Submit capture requests */
    INFO_PRINT("Starting repeat capture requests.\n");
    if (icapture_session_->repeat(request.get()) != Argus::STATUS_OK)
        ERROR_PRINT("Failed to start repeat capture request");

    while (!abort_) {
        sleep(1);
    }

    eglTerminate(egl_display);
}

void LibargusBufferCapturer::RunConsumer() {
    INFO_PRINT("Launching libargus consumer thread");

    /* Collect required event types */
    std::vector<Argus::EventType> eventTypes;
    eventTypes.push_back(Argus::EVENT_TYPE_CAPTURE_COMPLETE);
    eventTypes.push_back(Argus::EVENT_TYPE_ERROR);
    eventTypes.push_back(Argus::EVENT_TYPE_CAPTURE_STARTED);
    ev_queue_ = Argus::UniqueObj<Argus::EventQueue>(ievent_provider_->createEventQueue(eventTypes));
    iqueue_ = Argus::interface_cast<Argus::IEventQueue>(ev_queue_);

    consumer_ = std::make_unique<Worker>("LibargusConsumer", [this]() {
        CaptureImage();
    });
    consumer_->Run();
}

void LibargusBufferCapturer::CaptureImage() {
    Argus::Status argusStatus;
    ievent_provider_->waitForEvents(ev_queue_.get(), WAIT_FOR_EVENT_TIMEOUT);
    if (iqueue_->getSize() == 0 || abort_) {
        return;
    }

    const Argus::Event *event = iqueue_->getEvent(iqueue_->getSize() - 1);
    const Argus::IEvent *ievent = interface_cast<const Argus::IEvent>(event);
    if (!ievent) {
        ERROR_PRINT("Failed to get IEvent interface");
    }

    if (ievent->getEventType() == Argus::EVENT_TYPE_ERROR) {
        const Argus::IEventError *iEventError = interface_cast<const Argus::IEventError>(event);
        argusStatus = iEventError->getStatus();
        PrintErrorStatus(argusStatus);
        return;
    }

    if (!ibuffer_output_stream_)
        ERROR_PRINT("Failed to get IBufferOutputStream interface");

    /* Acquire a Buffer from a completed capture request */
    Argus::Buffer *buffer =
        ibuffer_output_stream_->acquireBuffer(ACQUIRE_FRAME_TIMEOUT, &argusStatus);
    if (argusStatus != Argus::STATUS_OK) {
        PrintErrorStatus(argusStatus);
        return;
    }

    /* Convert Argus::Buffer to DmaBuffer and get FD */
    auto dmabuf = DmaBuffer::fromArgusBuffer(buffer);
    int dmabuf_fd = dmabuf->getFd();
    frame_buffer_->SetDmaFd(dmabuf_fd);
    frame_buffer_->SetTimestamp(dmabuf->getTimeval());

    // NV12M to NV12
    NvBufSurface *nvbuf = nullptr;
    if (NvBufSurfaceFromFd(dmabuf_fd, reinterpret_cast<void **>(&nvbuf)) != 0) {
        return;
    }
    auto &surf = nvbuf->surfaceList[0];
    int offset = 0;
    for (int p = 0; p < surf.planeParams.num_planes; ++p) {
        if (NvBufSurfaceMap(nvbuf, 0, p, NVBUF_MAP_READ) != 0)
            continue;
        if (NvBufSurfaceSyncForCpu(nvbuf, 0, p) != 0) {
            NvBufSurfaceUnMap(nvbuf, 0, p);
            continue;
        }
        uint8_t *addr = static_cast<uint8_t *>(surf.mappedAddr.addr[p]);
        for (uint32_t row = 0; row < surf.planeParams.height[p]; ++row) {
            int row_size = surf.planeParams.width[p] * surf.planeParams.bytesPerPix[p];
            memcpy(frame_buffer_->MutableData() + offset, addr + row * surf.planeParams.pitch[p],
                   row_size);
            offset += row_size;
        }
        NvBufSurfaceUnMap(nvbuf, 0, p);
    }

    Next(frame_buffer_);

    ibuffer_output_stream_->releaseBuffer(dmabuf->getArgusBuffer());
}

Argus::SensorMode *LibargusBufferCapturer::FindBestSensorMode(Argus::CameraDevice *camera_device,
                                                              int req_width, int req_height,
                                                              int req_fps) {
    auto icamera_properties = interface_cast<Argus::ICameraProperties>(camera_device);
    std::vector<Argus::SensorMode *> sensor_modes;
    icamera_properties->getAllSensorModes(&sensor_modes);

    for (auto mode : sensor_modes) {
        auto imode = interface_cast<Argus::ISensorMode>(mode);
        if (!imode) {
            continue;
        }
        auto resolution = imode->getResolution();
        auto duration = imode->getFrameDurationRange();
        Argus::Range<uint64_t> desired(static_cast<uint64_t>(std::ceil(1e9 / req_fps)));
        if (resolution.width() == req_width && resolution.height() == req_height &&
            desired.max() >= duration.min() && desired.min() <= duration.max()) {
            return mode;
        }
    }
    return nullptr;
}

void LibargusBufferCapturer::PrintErrorStatus(Argus::Status status) {
    switch (status) {
        case Argus::STATUS_OK:
            ERROR_PRINT("Argus::STATUS_OK");
            break;
        case Argus::STATUS_INVALID_PARAMS:
            ERROR_PRINT("Argus::STATUS_INVALID_PARAMS");
            break;
        case Argus::STATUS_INVALID_SETTINGS:
            ERROR_PRINT("Argus::STATUS_INVALID_SETTINGS");
            break;
        case Argus::STATUS_UNAVAILABLE:
            ERROR_PRINT("Argus::STATUS_UNAVAILABLE");
            break;
        case Argus::STATUS_OUT_OF_MEMORY:
            ERROR_PRINT("Argus::STATUS_OUT_OF_MEMORY");
            break;
        case Argus::STATUS_UNIMPLEMENTED:
            ERROR_PRINT("Argus::STATUS_UNIMPLEMENTED");
            break;
        case Argus::STATUS_TIMEOUT:
            ERROR_PRINT("Argus::STATUS_TIMEOUT");
            break;
        case Argus::STATUS_CANCELLED:
            ERROR_PRINT("Argus::STATUS_CANCELLED");
            break;
        case Argus::STATUS_DISCONNECTED:
            ERROR_PRINT("Argus::STATUS_DISCONNECTED");
            break;
        case Argus::STATUS_END_OF_STREAM:
            ERROR_PRINT("Argus::STATUS_END_OF_STREAM");
            break;
        default:
            ERROR_PRINT("BAD STATUS");
            break;
    }
}
