#ifndef V4L2_FRAME_BUFFER_H_
#define V4L2_FRAME_BUFFER_H_

#include "common/v4l2_utils.h"

#include <linux/videodev2.h>
#include <vector>

#include <api/video/i420_buffer.h>
#include <api/video/video_frame.h>
#include <common_video/include/video_frame_buffer.h>
#include <rtc_base/memory/aligned_malloc.h>

class V4L2FrameBuffer : public webrtc::VideoFrameBuffer {
  public:
    static rtc::scoped_refptr<V4L2FrameBuffer> Create(int width, int height, int size,
                                                      uint32_t format);
    static rtc::scoped_refptr<V4L2FrameBuffer> Create(int width, int height, V4L2Buffer buffer);

    Type type() const override;
    int width() const override;
    int height() const override;
    rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;

    uint32_t format() const;
    uint32_t size() const;
    uint32_t flags() const;
    timeval timestamp() const;

    const void *Data() const;
    uint8_t *MutableData();
    V4L2Buffer GetRawBuffer();
    int GetDmaFd() const;
    bool SetDmaFd(int fd);
    rtc::scoped_refptr<V4L2FrameBuffer> Clone() const;

  protected:
    V4L2FrameBuffer(int width, int height, int size, uint32_t format);
    V4L2FrameBuffer(int width, int height, V4L2Buffer buffer);
    ~V4L2FrameBuffer() override;

  private:
    const int width_;
    const int height_;
    const uint32_t format_;
    uint32_t size_;
    uint32_t flags_;
    bool has_mutable_data_;
    timeval timestamp_;
    V4L2Buffer buffer_;
    const std::unique_ptr<uint8_t, webrtc::AlignedFreeDeleter> data_;
};

#endif // V4L2_FRAME_BUFFER_H_

using V4L2FrameBufferRef = rtc::scoped_refptr<V4L2FrameBuffer>;
