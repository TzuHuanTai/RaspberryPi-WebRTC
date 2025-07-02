#ifndef VIDEO_CAPTURER_H_
#define VIDEO_CAPTURER_H_

#include "args.h"
#include "common/interface/subject.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
#include <modules/video_capture/video_capture.h>

#include <variant>

class VideoCapturer {
  public:
    VideoCapturer() = default;
    ~VideoCapturer() { frame_buffer_subject_.UnSubscribe(); }

    virtual int fps() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool is_dma_capture() const = 0;
    virtual uint32_t format() const = 0;
    virtual Args config() const = 0;
    virtual void StartCapture() = 0;
    virtual rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame() = 0;

    virtual VideoCapturer &SetControls(int key, int value) { return *this; };

    std::shared_ptr<Observable<rtc::scoped_refptr<V4L2FrameBuffer>>> AsFrameBufferObservable() {
        return frame_buffer_subject_.AsObservable();
    }

  protected:
    void NextFrameBuffer(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
        frame_buffer_subject_.Next(frame_buffer);
    }

  private:
    Subject<rtc::scoped_refptr<V4L2FrameBuffer>> frame_buffer_subject_;
};

#endif
