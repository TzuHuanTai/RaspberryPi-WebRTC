#include "libargus_egl_capturer.h"

#include <iostream>
#include <sys/mman.h>

#include "common/logging.h"
#include "common/utils.h"

static constexpr uint64_t kAcquireFrameTimeoutNs = 3'000'000'000;

std::shared_ptr<LibargusEglCapturer> LibargusEglCapturer::Create(Args args) {
    auto ptr = std::make_shared<LibargusEglCapturer>(args);
    ptr->Initialize();
    ptr->StartCapture();
    return ptr;
}

LibargusEglCapturer::LibargusEglCapturer(Args args)
    : camera_id_(args.camera_id),
      num_streams_(args.num_streams),
      fps_(args.fps),
      format_(args.format),
      config_(args) {}

LibargusEglCapturer::~LibargusEglCapturer() {
    for (auto &worker : workers_) {
        worker.reset();
    }
    for (int fd : dma_fds_) {
        DestroyNvBufferFromFd(fd);
    }

    if (icapture_session_) {
        icapture_session_->stopRepeat();
        icapture_session_->waitForIdle();
    }

    camera_provider_.reset();
}

void LibargusEglCapturer::Initialize() {
    dma_fds_.assign(num_streams_, -1);
    output_streams_.resize(num_streams_);

    widths_.push_back(config_.width);
    heights_.push_back(config_.height);
    int frame_size =
        config_.width * config_.height + ((config_.width + 1) / 2) * ((config_.height + 1) / 2) * 2;
    framesizes_.push_back(frame_size);
    frame_buffer_.emplace_back(
        V4L2FrameBuffer::Create(config_.width, config_.height, frame_size, format_));

    if (has_sub_stream()) {
        widths_.push_back(config_.sub_width);
        heights_.push_back(config_.sub_height);
        int frame_size = config_.sub_width * config_.sub_height +
                         ((config_.sub_width + 1) / 2) * ((config_.sub_height + 1) / 2) * 2;
        framesizes_.push_back(frame_size);
        frame_buffer_.emplace_back(
            V4L2FrameBuffer::Create(config_.sub_width, config_.sub_height, frame_size, format_));
    }

    camera_provider_.reset(Argus::CameraProvider::create());
    if (!camera_provider_) {
        throw std::runtime_error("Failed to create CameraProvider");
    }

    InitStreams();
}

void LibargusEglCapturer::DestroyNvBufferFromFd(int &fd) {
    if (fd >= 0) {
        NvBufSurface *nvbuf = nullptr;
        if (NvBufSurfaceFromFd(fd, (void **)(&nvbuf)) == 0 && nvbuf) {
            NvBufSurfaceDestroy(nvbuf);
        }
        fd = -1;
    }
}

void LibargusEglCapturer::PrintSensorModeInfo(Argus::SensorMode *sensorMode, const char *indent) {
    Argus::ISensorMode *iSensorMode = Argus::interface_cast<Argus::ISensorMode>(sensorMode);
    if (iSensorMode) {
        Argus::Size2D<uint32_t> resolution = iSensorMode->getResolution();
        printf("%sResolution:         %ux%u\n", indent, resolution.width(), resolution.height());

        Argus::Range<float> hdrRatioRange = iSensorMode->getHdrRatioRange();
        printf("%sHdrRatioRange:  [%f, %f]\n", indent, static_cast<float>(hdrRatioRange.min()),
               static_cast<float>(hdrRatioRange.max()));

        if (hdrRatioRange.max() > 1.f) {
            Argus::Range<uint64_t> u64Range = iSensorMode->getExposureTimeRange();
            printf("%sExposureTimeRange for long exposure::  [%llu, %llu]\n", indent,
                   static_cast<unsigned long long>(u64Range.min()),
                   static_cast<unsigned long long>(u64Range.max()));

            printf("%sExposureTimeRange for short exposure: [%llu, %llu]\n", indent,
                   static_cast<unsigned long long>(u64Range.min() / hdrRatioRange.max()),
                   static_cast<unsigned long long>(u64Range.max() / hdrRatioRange.min()));
        } else {
            Argus::Range<uint64_t> u64Range = iSensorMode->getExposureTimeRange();
            printf("%sExposureTimeRange:  [%llu, %llu]\n", indent,
                   static_cast<unsigned long long>(u64Range.min()),
                   static_cast<unsigned long long>(u64Range.max()));
        }

        Argus::Range<uint64_t> u64Range = iSensorMode->getFrameDurationRange();
        printf("%sFrameDurationRange: [%llu, %llu]\n", indent,
               static_cast<unsigned long long>(u64Range.min()),
               static_cast<unsigned long long>(u64Range.max()));
        printf("%s                    (%.2f to %.2f fps)\n", indent,
               (1000000000.0 / u64Range.max()), (1000000000.0 / u64Range.min()));
        Argus::Range<float> fRange = iSensorMode->getAnalogGainRange();
        printf("%sAnalogGainRange:    [%f, %f]\n", indent, fRange.min(), fRange.max());
        printf("%sInputBitDepth:      %u\n", indent, iSensorMode->getInputBitDepth());
        printf("%sOutputBitDepth:     %u\n", indent, iSensorMode->getOutputBitDepth());
        printf("%sSensorModeType:     %s\n", indent, iSensorMode->getSensorModeType().getName());

        Argus::Ext::IDolWdrSensorMode *dolMode =
            Argus::interface_cast<Argus::Ext::IDolWdrSensorMode>(sensorMode);
        Argus::Ext::IPwlWdrSensorMode *pwlMode =
            Argus::interface_cast<Argus::Ext::IPwlWdrSensorMode>(sensorMode);
        if (dolMode) {
            printf("%sDOL WDR Mode Properties:\n", indent);
            printf("%s  ExposureCount:        %u\n", indent, dolMode->getExposureCount());
            printf("%s  OpticalBlackRowCount: %u\n", indent, dolMode->getOpticalBlackRowCount());
            std::vector<uint32_t> vbpRowCounts;
            dolMode->getVerticalBlankPeriodRowCount(&vbpRowCounts);
            printf("%s  VBPRowCounts:         [%u", indent, vbpRowCounts[0]);
            for (uint32_t i = 1; i < vbpRowCounts.size(); i++) {
                printf(", %u", vbpRowCounts[i]);
            }
            printf("]\n");
            printf("%s  LineInfoMarkerWidth:  %u\n", indent, dolMode->getLineInfoMarkerWidth());
            printf("%s  LeftMarginWidth:      %u\n", indent, dolMode->getLeftMarginWidth());
            printf("%s  RightMarginWidth:     %u\n", indent, dolMode->getRightMarginWidth());
            resolution = dolMode->getPhysicalResolution();
            printf("%s  PhysicalResolution:   %ux%u\n", indent, resolution.width(),
                   resolution.height());
        } else if (pwlMode) {
            printf("%sPWL WDR Mode Properties:\n", indent);
            printf("%s  ControlPointCount:    %u\n", indent, pwlMode->getControlPointCount());
            std::vector<Argus::Point2D<float>> controlPoints;
            pwlMode->getControlPoints(&controlPoints);
            printf("%s  ControlPoints:        {(%f, %f)", indent, controlPoints[0].x(),
                   controlPoints[0].y());
            for (uint32_t i = 1; i < controlPoints.size(); i++) {
                printf(", (%f, %f)", controlPoints[i].x(), controlPoints[i].y());
            }
            printf("}\n");
        } else {
            printf("%sIS WDR Mode: No\n", indent);
        }
    }
}

void LibargusEglCapturer::PrintCameraDeviceInfo(Argus::CameraDevice *cameraDevice,
                                                const char *indent) {
    Argus::ICameraProperties *iCameraProperties =
        Argus::interface_cast<Argus::ICameraProperties>(cameraDevice);
    if (iCameraProperties) {
        std::vector<Argus::SensorMode *> sensorModes;
        iCameraProperties->getAllSensorModes(&sensorModes);

        printf("%sMaxAeRegions:              %u\n", indent, iCameraProperties->getMaxAeRegions());
        printf("%sMaxAwbRegions:             %u\n", indent, iCameraProperties->getMaxAwbRegions());
        Argus::Range<int32_t> i32Range = iCameraProperties->getFocusPositionRange();
        printf("%sFocusPositionRange:        [%d, %d]\n", indent, i32Range.min(), i32Range.max());

        printf("%sLensApertureRange:\n", indent);
        std::vector<float> availableFnums;
        iCameraProperties->getAvailableApertureFNumbers(&availableFnums);
        for (std::vector<float>::iterator it = availableFnums.begin(); it != availableFnums.end();
             ++it) {
            printf("%s %f\n", indent, *it);
        }

        Argus::Range<float> fRange = iCameraProperties->getIspDigitalGainRange();
        printf("%sIspDigitalGainRange:       [%f, %f]\n", indent, fRange.min(), fRange.max());
        fRange = iCameraProperties->getExposureCompensationRange();
        printf("%sExposureCompensationRange: [%f, %f]\n", indent, fRange.min(), fRange.max());
        printf("%sNumSensorModes:            %lu\n", indent,
               static_cast<unsigned long>(sensorModes.size()));
        for (uint32_t i = 0; i < sensorModes.size(); i++) {
            printf("%sSensorMode %d:\n", indent, i);
            char modeIndent[32];
            snprintf(modeIndent, sizeof(modeIndent), "%s    ", indent);
            PrintSensorModeInfo(sensorModes[i], modeIndent);
        }
    }
}

void LibargusEglCapturer::InitStreams() {
    icamera_provider_ = Argus::interface_cast<Argus::ICameraProvider>(camera_provider_);
    if (!icamera_provider_) {
        throw std::runtime_error("Failed to create CameraProvider");
    }
    INFO_PRINT("Argus Version: %s\n", icamera_provider_->getVersion().c_str());

    std::vector<Argus::CameraDevice *> camera_devices;
    if (icamera_provider_->getCameraDevices(&camera_devices) != Argus::STATUS_OK ||
        camera_devices.empty()) {
        throw std::runtime_error("No cameras available");
    }
    if (camera_devices.size() <= camera_id_) {
        INFO_PRINT("Camera device %u requested but only %lu available.", camera_id_,
                   camera_devices.size());
        for (uint32_t i = 0; i < camera_devices.size(); i++) {
            INFO_PRINT("  ==== CameraDevice %u: ====", i);
            PrintCameraDeviceInfo(camera_devices[i], "    ");
        }
        return;
    }

    camera_device_ = camera_devices[camera_id_];
    auto capture_session(icamera_provider_->createCaptureSession(camera_device_));
    icapture_session_ = Argus::interface_cast<Argus::ICaptureSession>(capture_session);
    if (!icapture_session_) {
        throw std::runtime_error("Failed to get ICaptureSession");
    }

    Argus::UniqueObj<Argus::OutputStreamSettings> stream_settings(
        icapture_session_->createOutputStreamSettings(Argus::STREAM_TYPE_EGL));
    auto iegl_stream_settings = interface_cast<Argus::IEGLOutputStreamSettings>(stream_settings);
    if (!iegl_stream_settings) {
        throw std::runtime_error("Failed to get IEGLOutputStreamSettings");
    }
    for (size_t i = 0; i < num_streams_; i++) {
        iegl_stream_settings->setEGLDisplay(EGL_NO_DISPLAY);
        iegl_stream_settings->setPixelFormat(Argus::PIXEL_FMT_YCbCr_420_888);
        iegl_stream_settings->setResolution(Argus::Size2D<uint32_t>(widths_[i], heights_[i]));
        output_streams_[i].reset(icapture_session_->createOutputStream(stream_settings.get()));
    }

    request_ = Argus::UniqueObj<Argus::Request>(icapture_session_->createRequest());
    irequest_ = interface_cast<Argus::IRequest>(request_);
    for (size_t i = 0; i < num_streams_; i++) {
        irequest_->enableOutputStream(output_streams_[i].get());
        auto irequest_stream_settings_ = interface_cast<Argus::IStreamSettings>(
            irequest_->getStreamSettings(output_streams_[i].get()));
        if (!irequest_stream_settings_) {
            throw std::runtime_error("Failed to get IStreamSettings");
        }
        irequest_stream_settings_->setPostProcessingEnable(false);

        INFO_PRINT("stream %zu initialized: %dx%d", i, widths_[i], heights_[i]);
    }

    auto isource_settings = interface_cast<Argus::ISourceSettings>(irequest_->getSourceSettings());
    if (!isource_settings) {
        throw std::runtime_error("Failed to get ISourceSettings");
    }
    auto mode = FindBestSensorMode(widths_[0], heights_[0], fps_);
    isource_settings->setSensorMode(mode);
    isource_settings->setFrameDurationRange(
        Argus::Range<uint64_t>(static_cast<uint64_t>(1e9 / fps_)));

    for (size_t i = 0; i < num_streams_; i++) {
        iegl_streams_.push_back(interface_cast<Argus::IEGLOutputStream>(output_streams_[i].get()));
        frame_consumers_.emplace_back(EGLStream::FrameConsumer::create(output_streams_[i].get()));
        iframe_consumers_.push_back(
            interface_cast<EGLStream::IFrameConsumer>(frame_consumers_.back()));

        workers_.push_back(std::make_unique<Worker>("LibargusCapture", [this, i]() {
            if (iegl_streams_[i]->waitUntilConnected() != Argus::STATUS_OK) {
                ERROR_PRINT("Stream %zu failed to connect.", i);
                return;
            }
            while (true) {
                CaptureImage(i);
            }
        }));
    }

    if (icapture_session_->repeat(request_.get()) != Argus::STATUS_OK) {
        throw std::runtime_error("Failed to start repeat");
    }
}

int LibargusEglCapturer::fps() const { return fps_; }

int LibargusEglCapturer::width(int stream_idx) const {
    if (stream_idx >= num_streams_ || stream_idx < 0) {
        return widths_[0];
    }
    return widths_[stream_idx];
}

int LibargusEglCapturer::height(int stream_idx) const {
    if (stream_idx >= num_streams_ || stream_idx < 0) {
        return heights_[0];
    }
    return heights_[stream_idx];
}

bool LibargusEglCapturer::is_dma_capture() const { return false; }

uint32_t LibargusEglCapturer::format() const { return format_; }

Args LibargusEglCapturer::config() const { return config_; }

void LibargusEglCapturer::CaptureImage(int stream_idx) {
    Argus::Status status;
    Argus::UniqueObj<EGLStream::Frame> frame(
        iframe_consumers_[stream_idx]->acquireFrame(kAcquireFrameTimeoutNs, &status));
    if (status != Argus::STATUS_OK) {
        return;
    }

    auto iFrame = interface_cast<EGLStream::IFrame>(frame);
    auto image = iFrame->getImage();
    auto timestamp = Utils::ToTimeval(iFrame->getTime());

    auto native_buffer = interface_cast<EGLStream::NV::IImageNativeBuffer>(image);
    if (!native_buffer) {
        return;
    }
    if (dma_fds_[stream_idx] == -1) {
        dma_fds_[stream_idx] =
            native_buffer->createNvBuffer(iegl_streams_[stream_idx]->getResolution(),
                                          NVBUF_COLOR_FORMAT_YUV420, NVBUF_LAYOUT_PITCH);
        if (dma_fds_[stream_idx] < 0) {
            return;
        }
    }

    if (native_buffer->copyToNvBuffer(dma_fds_[stream_idx]) != Argus::STATUS_OK) {
        DestroyNvBufferFromFd(dma_fds_[stream_idx]);
        return;
    }

    frame_buffer_[stream_idx]->SetDmaFd(dma_fds_[stream_idx]);
    frame_buffer_[stream_idx]->SetTimestamp(timestamp);

    NvBufSurface *nvbuf = nullptr;
    if (NvBufSurfaceFromFd(dma_fds_[stream_idx], reinterpret_cast<void **>(&nvbuf)) != 0) {
        DestroyNvBufferFromFd(dma_fds_[stream_idx]);
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
            memcpy(frame_buffer_[stream_idx]->MutableData() + offset,
                   addr + row * surf.planeParams.pitch[p], row_size);
            offset += row_size;
        }
        NvBufSurfaceUnMap(nvbuf, 0, p);
    }

    if (stream_idx == 0) {
        Next(frame_buffer_[stream_idx]);
    }

    if (has_sub_stream() && stream_idx == 1) {
        // Notify sub stream observers
        sub_stream_.Next(frame_buffer_[stream_idx]);
    }
}

rtc::scoped_refptr<webrtc::I420BufferInterface> LibargusEglCapturer::GetI420Frame(int stream_idx) {
    if (stream_idx >= num_streams_ || stream_idx < 0) {
        return frame_buffer_[0]->ToI420();
    }
    return frame_buffer_[stream_idx]->ToI420();
}

void LibargusEglCapturer::StartCapture() {
    for (auto &worker : workers_) {
        worker->Run();
    }
}

Subscription LibargusEglCapturer::SubscribeSub(Subject<V4L2FrameBufferRef>::Callback callback) {
    return sub_stream_.Subscribe(std::move(callback));
}

Argus::SensorMode *LibargusEglCapturer::FindBestSensorMode(int req_width, int req_height,
                                                           int req_fps) {
    auto icamera_properties = interface_cast<Argus::ICameraProperties>(camera_device_);
    std::vector<Argus::SensorMode *> sensor_modes;
    icamera_properties->getAllSensorModes(&sensor_modes);

    for (auto mode : sensor_modes) {
        auto imode = interface_cast<Argus::ISensorMode>(mode);
        if (!imode) {
            continue;
        }
        auto resolution = imode->getResolution();
        auto duration = imode->getFrameDurationRange();
        Argus::Range<uint64_t> desired(static_cast<uint64_t>(1e9 / req_fps + 0.5));
        if (resolution.width() == req_width && resolution.height() == req_height &&
            desired.max() >= duration.min() && desired.min() <= duration.max()) {
            return mode;
        }
    }
    return nullptr;
}
