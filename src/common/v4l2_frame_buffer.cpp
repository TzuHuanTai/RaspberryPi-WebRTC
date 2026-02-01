#include "common/v4l2_frame_buffer.h"
#include "common/logging.h"

#include <third_party/libyuv/include/libyuv.h>

#include <chrono>

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

rtc::scoped_refptr<V4L2FrameBuffer> V4L2FrameBuffer::Create(int width, int height, int size,
                                                            uint32_t format) {
    return rtc::make_ref_counted<V4L2FrameBuffer>(width, height, size, format);
}

rtc::scoped_refptr<V4L2FrameBuffer> V4L2FrameBuffer::Create(int width, int height,
                                                            V4L2Buffer buffer) {
    return rtc::make_ref_counted<V4L2FrameBuffer>(width, height, buffer);
}

V4L2FrameBuffer::V4L2FrameBuffer(int width, int height, uint32_t format, int size, uint32_t flags,
                                 timeval timestamp)
    : width_(width),
      height_(height),
      format_(format),
      size_(size),
      flags_(flags),
      timestamp_(timestamp),
      buffer_({}),
      data_(nullptr) {}

V4L2FrameBuffer::V4L2FrameBuffer(int width, int height, V4L2Buffer buffer)
    : V4L2FrameBuffer(width, height, buffer.pix_fmt, buffer.length, buffer.flags,
                      buffer.timestamp) {
    buffer_ = buffer;
}

V4L2FrameBuffer::V4L2FrameBuffer(int width, int height, int size, uint32_t format)
    : V4L2FrameBuffer(width, height, format, size, 0, {0, 0}) {
    data_.reset(static_cast<uint8_t *>(webrtc::AlignedMalloc(size_, kBufferAlignment)));
}

V4L2FrameBuffer::~V4L2FrameBuffer() {}

webrtc::VideoFrameBuffer::Type V4L2FrameBuffer::type() const { return Type::kNative; }

int V4L2FrameBuffer::width() const { return width_; }
int V4L2FrameBuffer::height() const { return height_; }
uint32_t V4L2FrameBuffer::format() const { return format_; }
uint32_t V4L2FrameBuffer::size() const { return size_; }
uint32_t V4L2FrameBuffer::flags() const { return flags_; }
timeval V4L2FrameBuffer::timestamp() const { return timestamp_; }

rtc::scoped_refptr<webrtc::I420BufferInterface> V4L2FrameBuffer::ToI420() {
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(webrtc::I420Buffer::Create(width_, height_));
    i420_buffer->InitializeData();

    const uint8_t *src = static_cast<const uint8_t *>(Data());

    if (format_ == V4L2_PIX_FMT_YUV420) {
        memcpy(i420_buffer->MutableDataY(), src, size_);
    } else if (format_ == V4L2_PIX_FMT_H264) {
        // use hw decoded frame from track.
    } else {
        if (libyuv::ConvertToI420(src, size_, i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                                  i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                                  i420_buffer->MutableDataV(), i420_buffer->StrideV(), 0, 0, width_,
                                  height_, width_, height_, libyuv::kRotate0, format_) < 0) {
            ERROR_PRINT("libyuv ConvertToI420 Failed");
        }
    }

    return i420_buffer;
}

V4L2Buffer V4L2FrameBuffer::GetRawBuffer() { return buffer_; }

const void *V4L2FrameBuffer::Data() const { return data_ ? data_.get() : buffer_.start; }

uint8_t *V4L2FrameBuffer::MutableData() {
    if (!data_) {
        throw std::runtime_error(
            "MutableData() is not supported for frames directly created from V4L2 buffers. Use "
            "Clone() to create an owning (writable) copy before calling MutableData().");
    }
    return data_.get();
}

int V4L2FrameBuffer::GetDmaFd() const { return buffer_.dmafd; }

void V4L2FrameBuffer::SetDmaFd(int fd) {
    if (fd > 0) {
        buffer_.dmafd = fd;
    }
}

void V4L2FrameBuffer::SetTimestamp(timeval timestamp) { timestamp_ = timestamp; }

rtc::scoped_refptr<V4L2FrameBuffer> V4L2FrameBuffer::Clone() const {
    auto clone = rtc::make_ref_counted<V4L2FrameBuffer>(width_, height_, size_, format_);

    memcpy(clone->MutableData(), Data(), size_);

    clone->SetDmaFd(buffer_.dmafd);
    clone->flags_ = flags_;
    clone->timestamp_ = timestamp_;

    return clone;
}
