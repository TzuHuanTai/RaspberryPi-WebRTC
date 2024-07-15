#ifndef SWSCALE_TRACK_SOURCE_H_
#define SWSCALE_TRACK_SOURCE_H_

#include "common/v4l2_utils.h"
#include "capture/v4l2_capture.h"

#include <media/base/adapted_video_track_source.h>

class SwScaleTrackSource : public rtc::AdaptedVideoTrackSource {
public:
    int width_;
    int height_;
    int config_width_;
    int config_height_;
    webrtc::VideoType src_video_type_;

    static rtc::scoped_refptr<SwScaleTrackSource> Create(std::shared_ptr<V4L2Capture> capture);
    SwScaleTrackSource(std::shared_ptr<V4L2Capture> capture);
    ~SwScaleTrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    void StartTrack();
    rtc::scoped_refptr<webrtc::I420BufferInterface> GetI420Frame();

protected:
    std::shared_ptr<V4L2Capture> capture_;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> i420_raw_buffer_;

    virtual void OnFrameCaptured(rtc::scoped_refptr<V4l2FrameBuffer> &frame_buffer);
};

#endif
