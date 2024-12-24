#ifndef SCALE_TRACK_SOURCE_H_
#define SCALE_TRACK_SOURCE_H_

#include <media/base/adapted_video_track_source.h>
#include <rtc_base/timestamp_aligner.h>

#include "capturer/video_capturer.h"

class ScaleTrackSource : public rtc::AdaptedVideoTrackSource {
  public:
    static rtc::scoped_refptr<ScaleTrackSource> Create(std::shared_ptr<VideoCapturer> capturer);
    ScaleTrackSource(std::shared_ptr<VideoCapturer> capturer);
    ~ScaleTrackSource();

    SourceState state() const override;
    bool remote() const override;
    bool is_screencast() const override;
    absl::optional<bool> needs_denoising() const override;
    virtual void StartTrack();

  protected:
    int width;
    int height;
    std::shared_ptr<VideoCapturer> capturer;
    rtc::TimestampAligner timestamp_aligner;

  private:
    void OnFrameCaptured(rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer);
};

#endif
