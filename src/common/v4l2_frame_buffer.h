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
    unsigned int size() const;
    unsigned int flags() const;
    timeval timestamp() const;

    void CopyBufferData();
    const void *Data() const;
    V4L2Buffer GetRawBuffer();

  protected:
    V4L2FrameBuffer(int width, int height, int size, uint32_t format);
    V4L2FrameBuffer(int width, int height, V4L2Buffer buffer);
    ~V4L2FrameBuffer() override;

  private:
    const int width_;
    const int height_;
    const uint32_t format_;
    unsigned int size_;
    unsigned int flags_;
    bool is_buffer_copied;
    timeval timestamp_;
    V4L2Buffer buffer_;
    const std::unique_ptr<uint8_t, webrtc::AlignedFreeDeleter> data_;
};

#endif // V4L2_FRAME_BUFFER_H_
