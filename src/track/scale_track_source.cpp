#include "track/scale_track_source.h"

// WebRTC
#include <api/video/i420_buffer.h>
#include <third_party/libyuv/include/libyuv.h>

#include "common/logging.h"
#include "common/v4l2_frame_buffer.h"

static const int kBufferAlignment = 64;

rtc::scoped_refptr<ScaleTrackSource>
ScaleTrackSource::Create(std::shared_ptr<VideoCapturer> capturer) {
    auto obj = rtc::make_ref_counted<ScaleTrackSource>(std::move(capturer));
    obj->StartTrack();
    return obj;
}

ScaleTrackSource::ScaleTrackSource(std::shared_ptr<VideoCapturer> capturer)
    : capturer(capturer),
      width(capturer->width()),
      height(capturer->height()),
      stream_idx(capturer->config().live_stream_idx) {}

ScaleTrackSource::~ScaleTrackSource() {
    // todo: tell capture unsubscribe observer.
}

void ScaleTrackSource::StartTrack() {
    subscription_ = capturer->Subscribe(
        [this](rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer) {
            OnFrameCaptured(frame_buffer);
        },
        stream_idx);
}

void ScaleTrackSource::OnFrameCaptured(rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer) {
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
    if (capturer->config().no_adaptive) {
        adapted_width = width;
        adapted_height = height;
    } else if (!AdaptFrame(width, height, timestamp_us, &adapted_width, &adapted_height,
                           &crop_width, &crop_height, &crop_x, &crop_y)) {
        return;
    }

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> dst_buffer = frame_buffer;

    if (adapted_width != width || adapted_height != height) {
        int dst_stride = std::ceil((double)adapted_width / kBufferAlignment) * kBufferAlignment;
        auto i420_buffer = webrtc::I420Buffer::Create(adapted_width, adapted_height, dst_stride,
                                                      dst_stride / 2, dst_stride / 2);
        i420_buffer->ScaleFrom(*frame_buffer->ToI420());
        dst_buffer = i420_buffer;
    }

    OnFrame(webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(dst_buffer)
                .set_rotation(webrtc::kVideoRotation_0)
                .set_timestamp_us(translated_timestamp_us)
                .build());
}

webrtc::MediaSourceInterface::SourceState ScaleTrackSource::state() const {
    return SourceState::kLive;
}

bool ScaleTrackSource::remote() const { return false; }

bool ScaleTrackSource::is_screencast() const { return false; }

absl::optional<bool> ScaleTrackSource::needs_denoising() const { return false; }
