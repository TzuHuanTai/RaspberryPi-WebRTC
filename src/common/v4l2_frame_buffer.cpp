#include "common/v4l2_frame_buffer.h"
#include "common/logging.h"

#include <third_party/libyuv/include/libyuv.h>

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
      is_buffer_copied(false),
      data_(static_cast<uint8_t *>(webrtc::AlignedMalloc(size_, kBufferAlignment))) {}

V4L2FrameBuffer::V4L2FrameBuffer(int width, int height, int size, uint32_t format)
    : width_(width),
      height_(height),
      format_(format),
      size_(size),
      flags_(0),
      timestamp_({0, 0}),
      is_buffer_copied(false),
      data_(static_cast<uint8_t *>(webrtc::AlignedMalloc(size_, kBufferAlignment))) {}

V4L2FrameBuffer::~V4L2FrameBuffer() {}

webrtc::VideoFrameBuffer::Type V4L2FrameBuffer::type() const { return Type::kNative; }

int V4L2FrameBuffer::width() const { return width_; }

int V4L2FrameBuffer::height() const { return height_; }

uint32_t V4L2FrameBuffer::format() const { return format_; }

unsigned int V4L2FrameBuffer::size() const { return size_; }

unsigned int V4L2FrameBuffer::flags() const { return flags_; }

timeval V4L2FrameBuffer::timestamp() const { return timestamp_; }

rtc::scoped_refptr<webrtc::I420BufferInterface> V4L2FrameBuffer::ToI420() {
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer(webrtc::I420Buffer::Create(width_, height_));
    i420_buffer->InitializeData();

    if (format_ == V4L2_PIX_FMT_YUV420) {
        memcpy(i420_buffer->MutableDataY(),
               is_buffer_copied ? data_.get() : (uint8_t *)buffer_.start, size_);
    } else if (format_ == V4L2_PIX_FMT_H264) {
        // use hw decoded frame from track.
    } else {
        if (libyuv::ConvertToI420(is_buffer_copied ? data_.get() : (uint8_t *)buffer_.start, size_,
                                  i420_buffer.get()->MutableDataY(), i420_buffer.get()->StrideY(),
                                  i420_buffer.get()->MutableDataU(), i420_buffer.get()->StrideU(),
                                  i420_buffer.get()->MutableDataV(), i420_buffer.get()->StrideV(),
                                  0, 0, width_, height_, width_, height_, libyuv::kRotate0,
                                  format_) < 0) {
            ERROR_PRINT("Mjpeg ConvertToI420 Failed");
        }
    }

    return i420_buffer;
}

void V4L2FrameBuffer::CopyBufferData() {
    memcpy(data_.get(), (uint8_t *)buffer_.start, size_);
    is_buffer_copied = true;
}

V4L2Buffer V4L2FrameBuffer::GetRawBuffer() { return buffer_; }

const void *V4L2FrameBuffer::Data() const { return data_.get(); }

uint8_t *V4L2FrameBuffer::MutableData() {
    is_buffer_copied = true;
    return data_.get();
}
