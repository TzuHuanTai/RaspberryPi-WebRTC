#include "v4l2_capturer.h"

// Linux
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/select.h>

// WebRTC
#include <modules/video_capture/video_capture_factory.h>
#include <third_party/libyuv/include/libyuv.h>

#include "common/logging.h"

std::shared_ptr<V4l2Capturer> V4l2Capturer::Create(Args args) {
    auto ptr = std::make_shared<V4l2Capturer>(args);
    ptr->Init(args.device);
    ptr->SetFps(args.fps)
        .SetRotation(args.rotation_angle)
        .SetFormat(args.width, args.height)
        .StartCapture();
    return ptr;
}

V4l2Capturer::V4l2Capturer(Args args)
    : buffer_count_(4),
      hw_accel_(args.hw_accel),
      format_(args.format),
      has_first_keyframe_(false),
      config_(args) {}

void V4l2Capturer::Init(std::string device) {
    fd_ = V4l2Util::OpenDevice(device.c_str());

    if (!V4l2Util::InitBuffer(fd_, &capture_, V4L2_BUF_TYPE_VIDEO_CAPTURE, V4L2_MEMORY_MMAP)) {
        exit(0);
    }
}

V4l2Capturer::~V4l2Capturer() {
    worker_.reset();
    decoder_.reset();
    V4l2Util::StreamOff(fd_, capture_.type);
    V4l2Util::DeallocateBuffer(fd_, &capture_);
    V4l2Util::CloseDevice(fd_);
}

int V4l2Capturer::fps() const { return fps_; }

int V4l2Capturer::width() const { return width_; }

int V4l2Capturer::height() const { return height_; }

bool V4l2Capturer::is_dma_capture() const { return hw_accel_ && IsCompressedFormat(); }

uint32_t V4l2Capturer::format() const { return format_; }

Args V4l2Capturer::config() const { return config_; }

bool V4l2Capturer::IsCompressedFormat() const {
    return format_ == V4L2_PIX_FMT_MJPEG || format_ == V4L2_PIX_FMT_H264;
}

bool V4l2Capturer::CheckMatchingDevice(std::string unique_name) {
    struct v4l2_capability cap;
    if (V4l2Util::QueryCapabilities(fd_, &cap) && cap.bus_info[0] != 0 &&
        strcmp((const char *)cap.bus_info, unique_name.c_str()) == 0) {
        return true;
    }
    return false;
}

int V4l2Capturer::GetCameraIndex(webrtc::VideoCaptureModule::DeviceInfo *device_info) {
    for (int i = 0; i < device_info->NumberOfDevices(); i++) {
        char device_name[256];
        char unique_name[256];
        if (device_info->GetDeviceName(static_cast<uint32_t>(i), device_name, sizeof(device_name),
                                       unique_name, sizeof(unique_name)) == 0 &&
            CheckMatchingDevice(unique_name)) {
            DEBUG_PRINT("GetDeviceName(%d): device_name=%s, unique_name=%s", i, device_name,
                        unique_name);
            return i;
        }
    }
    return -1;
}

V4l2Capturer &V4l2Capturer::SetFormat(int width, int height) {
    width_ = width;
    height_ = height;

    V4l2Util::SetFormat(fd_, &capture_, width, height, format_);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 10000 * 1000);

    if (format_ == V4L2_PIX_FMT_H264) {
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
                             V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE,
                             V4L2_MPEG_VIDEO_H264_PROFILE_HIGH);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, 60); /* trick */
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
        V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, 2500 * 1000);
    }
    return *this;
}

V4l2Capturer &V4l2Capturer::SetFps(int fps) {
    fps_ = fps;
    DEBUG_PRINT("  Fps: %d", fps);
    if (!V4l2Util::SetFps(fd_, capture_.type, fps)) {
        exit(0);
    }
    return *this;
}

V4l2Capturer &V4l2Capturer::SetRotation(int angle) {
    DEBUG_PRINT("  Rotation: %d", angle);
    V4l2Util::SetCtrl(fd_, V4L2_CID_ROTATE, angle);
    return *this;
}

void V4l2Capturer::CaptureImage() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval tv = {};
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    int r = select(fd_ + 1, &fds, NULL, NULL, &tv);
    if (r == -1) {
        ERROR_PRINT("select failed");
        return;
    } else if (r == 0) { // timeout
        DEBUG_PRINT("capture timeout");
        return;
    }

    v4l2_buffer buf = {};
    buf.type = capture_.type;
    buf.memory = capture_.memory;

    if (!V4l2Util::DequeueBuffer(fd_, &buf)) {
        return;
    }

    V4l2Buffer buffer((uint8_t *)capture_.buffers[buf.index].start, buf.bytesused, buf.flags,
                      buf.timestamp);
    NextBuffer(buffer);

    if (!V4l2Util::QueueBuffer(fd_, &buf)) {
        return;
    }
}

rtc::scoped_refptr<webrtc::I420BufferInterface> V4l2Capturer::GetI420Frame() {
    return frame_buffer_->ToI420();
}

void V4l2Capturer::NextBuffer(V4l2Buffer &buffer) {
    if (hw_accel_) {
        // hardware encoding
        if (!has_first_keyframe_) {
            has_first_keyframe_ = (buffer.flags & V4L2_BUF_FLAG_KEYFRAME) != 0;
        }

        if (IsCompressedFormat()) {
            decoder_->EmplaceBuffer(buffer, [this](V4l2Buffer decoded_buffer) {
                frame_buffer_ =
                    V4l2FrameBuffer::Create(width_, height_, decoded_buffer, V4L2_PIX_FMT_YUV420);
                NextFrameBuffer(frame_buffer_);
            });
        } else {
            frame_buffer_ = V4l2FrameBuffer::Create(width_, height_, buffer, format_);
            NextFrameBuffer(frame_buffer_);
        }
    } else {
        // software decoding
        if (format_ != V4L2_PIX_FMT_H264) {
            frame_buffer_ = V4l2FrameBuffer::Create(width_, height_, buffer, format_);
            NextFrameBuffer(frame_buffer_);
        } else {
            // todo: h264 decoding
            INFO_PRINT("Software decoding h264 camera source is not support now.");
            exit(1);
        }
    }

    NextRawBuffer(buffer);
}

void V4l2Capturer::StartCapture() {
    if (!V4l2Util::AllocateBuffer(fd_, &capture_, buffer_count_) ||
        !V4l2Util::QueueBuffers(fd_, &capture_)) {
        exit(0);
    }

    V4l2Util::StreamOn(fd_, capture_.type);

    if (hw_accel_ && IsCompressedFormat()) {
        decoder_ = std::make_unique<V4l2Decoder>();
        decoder_->Configure(config_.width, config_.height, format_, true);
        decoder_->Start();
    }

    worker_ = std::make_unique<Worker>("V4l2Capture", [this]() {
        CaptureImage();
    });
    worker_->Run();
}
