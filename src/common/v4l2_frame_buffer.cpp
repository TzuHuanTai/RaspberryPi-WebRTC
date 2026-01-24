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

V4L2FrameBuffer::V4L2FrameBuffer(int width, int height, V4L2Buffer buffer)
    : width_(width),
      height_(height),
      format_(buffer.pix_fmt),
      size_(buffer.length),
      flags_(buffer.flags),
      timestamp_(buffer.timestamp),
      buffer_(buffer),
      has_mutable_data_(false),
      data_(nullptr) {}

V4L2FrameBuffer::V4L2FrameBuffer(int width, int height, int size, uint32_t format)
    : width_(width),
      height_(height),
      format_(format),
      size_(size),
      flags_(0),
      timestamp_({0, 0}),
      buffer_({}),
      has_mutable_data_(true),
      data_(static_cast<uint8_t *>(webrtc::AlignedMalloc(size_, kBufferAlignment))) {}

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

    if (format_ == V4L2_PIX_FMT_YUV420) {
        memcpy(i420_buffer->MutableDataY(),
               has_mutable_data_ ? data_.get() : (uint8_t *)buffer_.start, size_);
    } else if (format_ == V4L2_PIX_FMT_H264) {
        // use hw decoded frame from track.
    } else {
        if (libyuv::ConvertToI420(has_mutable_data_ ? data_.get() : (uint8_t *)buffer_.start, size_,
                                  i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                                  i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                                  i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(),
                                  0, 0, width_, height_, width_, height_, libyuv::kRotate0,
                                  format_) < 0) {
            ERROR_PRINT("libyuv ConvertToI420 Failed");
        }
    }

    return i420_buffer;
}

V4L2Buffer V4L2FrameBuffer::GetRawBuffer() { return buffer_; }

const void *V4L2FrameBuffer::Data() const {
    if (has_mutable_data_) {
        return data_.get();
    }
    return buffer_.start;
}

uint8_t *V4L2FrameBuffer::MutableData() {
    if (!has_mutable_data_) {
        throw std::runtime_error(
            "MutableData() is not supported for frames directly created from V4L2 buffers. Use "
            "Clone() to create an owning (writable) copy before calling MutableData().");
    }
    return data_.get();
}

int V4L2FrameBuffer::GetDmaFd() const { return buffer_.dmafd; }

bool V4L2FrameBuffer::SetDmaFd(int fd) {
    if (fd <= 0) {
        return false;
    }

    buffer_.dmafd = fd;
    return true;
}

void V4L2FrameBuffer::SetTimestamp(timeval timestamp) { timestamp_ = timestamp; }

/*  Return a new refptr with copied metadata and frame data. */
rtc::scoped_refptr<V4L2FrameBuffer> V4L2FrameBuffer::Clone() const {
    auto clone = rtc::make_ref_counted<V4L2FrameBuffer>(width_, height_, size_, format_);

    if (has_mutable_data_) {
        memcpy(clone->data_.get(), data_.get(), size_);
    } else {
        memcpy(clone->data_.get(), buffer_.start, size_);
    }
    clone->SetDmaFd(buffer_.dmafd);
    clone->flags_ = flags_;
    clone->timestamp_ = timestamp_;

    return clone;
}
