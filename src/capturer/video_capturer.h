#ifndef VIDEO_CAPTURER_H_
#define VIDEO_CAPTURER_H_

#include "args.h"
#include "common/interface/subject.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
#include <modules/video_capture/video_capture.h>

#include <libcamera/libcamera.h>

#include <variant>

class VideoCapturer {
  public:
    using ControlValue =
        std::variant<int, signed long, float, bool, libcamera::Span<const float, 2UL>,
                     libcamera::Span<const libcamera::Rectangle>>;

    VideoCapturer() = default;
    ~VideoCapturer() {
        raw_buffer_subject_.UnSubscribe();
        frame_buffer_subject_.UnSubscribe();
    }

    virtual int fps() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool is_dma_capture() const = 0;
    virtual uint32_t format() const = 0;
    virtual Args config() const = 0;
    virtual void StartCapture() = 0;
    virtual rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame() = 0;

    virtual VideoCapturer &SetResolution(int width, int height) = 0;
    virtual VideoCapturer &SetFps(int fps) = 0;
    virtual VideoCapturer &SetRotation(int angle) = 0;
    virtual VideoCapturer &SetControls(int key, ControlValue value) = 0;

    std::shared_ptr<Observable<V4L2Buffer>> AsRawBufferObservable() {
        return raw_buffer_subject_.AsObservable();
    }

    std::shared_ptr<Observable<rtc::scoped_refptr<V4L2FrameBuffer>>> AsFrameBufferObservable() {
        return frame_buffer_subject_.AsObservable();
    }

  protected:
    void NextRawBuffer(V4L2Buffer raw_buffer) { raw_buffer_subject_.Next(raw_buffer); }

    void NextFrameBuffer(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
        frame_buffer_subject_.Next(frame_buffer);
    }

  private:
    Subject<V4L2Buffer> raw_buffer_subject_;
    Subject<rtc::scoped_refptr<V4L2FrameBuffer>> frame_buffer_subject_;
};

#endif
