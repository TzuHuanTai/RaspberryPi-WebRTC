#include "libcamera_capturer.h"

#include <sys/mman.h>

#include "common/logging.h"

std::shared_ptr<LibcameraCapturer> LibcameraCapturer::Create(Args args) {
    auto ptr = std::make_shared<LibcameraCapturer>(args);
    ptr->Init(args.cameraId);
    ptr->SetFps(args.fps)
        .SetRotation(args.rotation_angle)
        .SetFormat(args.width, args.height)
        .StartCapture();
    return ptr;
}

LibcameraCapturer::LibcameraCapturer(Args args)
    : buffer_count_(2),
      format_(args.format),
      config_(args) {}

void LibcameraCapturer::Init(int deviceId) {
    cm_ = std::make_unique<libcamera::CameraManager>();
    int ret = cm_->start();
    if (ret) {
        throw std::runtime_error("Failed to start camera manager");
    }

    auto cameras = cm_->cameras();
    if (cameras.size() == 0) {
        throw std::runtime_error("No camera is available via libcamera.");
    }

    if (config_.cameraId >= cameras.size()) {
        throw std::runtime_error("Selected camera is not available.");
    }

    std::string const &cam_id = cameras[config_.cameraId]->id();
    INFO_PRINT("camera id: %s", cam_id.c_str());
    camera_ = cm_->get(cam_id);
    camera_->acquire();
    camera_config_ = camera_->generateConfiguration({libcamera::StreamRole::VideoRecording});
}

LibcameraCapturer::~LibcameraCapturer() {
    camera_->stop();
    allocator_->free(stream_);
    allocator_.reset();
    camera_config_.reset();
    camera_->release();
    camera_.reset();
    cm_->stop();
}

int LibcameraCapturer::fps() const { return fps_; }

int LibcameraCapturer::width() const { return width_; }

int LibcameraCapturer::height() const { return height_; }

bool LibcameraCapturer::is_dma_capture() const { return false; }

uint32_t LibcameraCapturer::format() const { return format_; }

Args LibcameraCapturer::config() const { return config_; }

LibcameraCapturer &LibcameraCapturer::SetFormat(int width, int height) {
    DEBUG_PRINT("camera original format: %s", camera_config_->at(0).toString().c_str());

    if (width && height) {
        libcamera::Size size(width, height);
        camera_config_->at(0).size = size;
    }

    camera_config_->at(0).pixelFormat = libcamera::formats::YUV420;
    camera_config_->at(0).bufferCount = buffer_count_;

    auto validation = camera_config_->validate();
    if (validation == libcamera::CameraConfiguration::Status::Valid) {
        INFO_PRINT("camera validated format: %s.", camera_config_->at(0).toString().c_str());
    } else if (validation == libcamera::CameraConfiguration::Status::Adjusted) {
        INFO_PRINT("camera adjusted format: %s.", camera_config_->at(0).toString().c_str());
    } else {
        ERROR_PRINT("Failed to validate camera configuration.");
        exit(1);
    }

    width_ = camera_config_->at(0).size.width;
    height_ = camera_config_->at(0).size.height;
    stride_ = camera_config_->at(0).stride;

    INFO_PRINT("  width: %d, height: %d, stride: %d", width_, height_, stride_);

    if (width_ != stride_) {
        ERROR_PRINT("Stride is not equal to width");
        exit(1);
    }

    return *this;
}

LibcameraCapturer &LibcameraCapturer::SetFps(int fps) {
    fps_ = fps;
    int64_t frame_time = 1000000 / fps;
    controls_.set(libcamera::controls::FrameDurationLimits,
                  libcamera::Span<const int64_t, 2>({frame_time, frame_time}));
    DEBUG_PRINT("  Fps: %d", fps);

    return *this;
}

LibcameraCapturer &LibcameraCapturer::SetControls(const int key, const int value) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    DEBUG_PRINT("Set camera controls, key: %d, value: %d", key, value);
    controls_.set(key, value);
    return *this;
}

LibcameraCapturer &LibcameraCapturer::SetRotation(int angle) {
    if (angle == 90) {
        camera_config_->orientation = libcamera::Orientation::Rotate90;
    } else if (angle == 180) {
        camera_config_->orientation = libcamera::Orientation::Rotate180;
    } else if (angle == 270) {
        camera_config_->orientation = libcamera::Orientation::Rotate270;
    }

    DEBUG_PRINT("  Rotation: %d", angle);

    return *this;
}

void LibcameraCapturer::AllocateBuffer() {
    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);

    stream_ = camera_config_->at(0).stream();
    int ret = allocator_->allocate(stream_);
    if (ret < 0) {
        ERROR_PRINT("Can't allocate buffers");
    }

    auto &buffers = allocator_->buffers(stream_);
    if (buffer_count_ != buffers.size()) {
        ERROR_PRINT("Buffer counts not match allocated buffer number");
        exit(1);
    }

    for (unsigned int i = 0; i < buffer_count_; i++) {
        auto &buffer = buffers[i];
        int fd = 0;
        int buffer_length = 0;
        for (auto &plane : buffer->planes()) {
            fd = plane.fd.get();
            buffer_length += plane.length;
        }
        void *memory = mmap(NULL, buffer_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        mapped_buffers_[fd] = std::make_pair(memory, buffer_length);
        DEBUG_PRINT("Allocated fd(%d) Buffer[%d] pointer: %p, length: %d", fd, i, memory,
                    buffer_length);

        auto request = camera_->createRequest();
        if (!request) {
            ERROR_PRINT("Can't create camera request");
        }
        int ret = request->addBuffer(stream_, buffer.get());
        if (ret < 0) {
            ERROR_PRINT("Can't set buffer for request");
        }
        requests_.push_back(std::move(request));
    }
}

void LibcameraCapturer::RequestComplete(libcamera::Request *request) {
    if (request->status() == libcamera::Request::RequestCancelled) {
        DEBUG_PRINT("Request has been cancelled");
        exit(1);
    }

    auto &buffers = request->buffers();
    auto *buffer = buffers.begin()->second;

    auto &plane = buffer->planes()[0];
    int fd = plane.fd.get();
    void *data = mapped_buffers_[fd].first;
    int length = mapped_buffers_[fd].second;
    timeval tv = {};
    tv.tv_sec = buffer->metadata().timestamp / 1000000000;
    tv.tv_usec = (buffer->metadata().timestamp % 1000000000) / 1000;

    V4l2Buffer v4l2_buffer((uint8_t *)data, length, V4L2_BUF_FLAG_KEYFRAME, tv);
    NextBuffer(v4l2_buffer);

    request->reuse(libcamera::Request::ReuseBuffers);

    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        request->controls() = controls_;
    }

    camera_->queueRequest(request);
}

rtc::scoped_refptr<webrtc::I420BufferInterface> LibcameraCapturer::GetI420Frame() {
    return frame_buffer_->ToI420();
}

void LibcameraCapturer::NextBuffer(V4l2Buffer &buffer) {
    frame_buffer_ = V4l2FrameBuffer::Create(width_, height_, buffer, format_);
    NextFrameBuffer(frame_buffer_);
    NextRawBuffer(buffer);
}

void LibcameraCapturer::StartCapture() {
    int ret = camera_->configure(camera_config_.get());
    if (ret < 0) {
        ERROR_PRINT("Failed to configure camera");
        exit(1);
    }

    AllocateBuffer();

    ret = camera_->start(&controls_);
    if (ret) {
        ERROR_PRINT("Failed to start capturing");
        exit(1);
    }

    controls_.clear();
    camera_->requestCompleted.connect(this, &LibcameraCapturer::RequestComplete);

    for (auto &request : requests_) {
        ret = camera_->queueRequest(request.get());
        if (ret < 0) {
            ERROR_PRINT("Can't queue request");
            camera_->stop();
        }
    }
}
