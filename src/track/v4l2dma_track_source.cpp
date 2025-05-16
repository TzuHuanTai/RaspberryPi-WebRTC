#include "track/v4l2dma_track_source.h"

#include <future>

// WebRTC
#include <api/video/i420_buffer.h>
#include <rtc_base/timestamp_aligner.h>
#include <third_party/libyuv/include/libyuv.h>

#include "common/v4l2_utils.h"

rtc::scoped_refptr<V4L2DmaTrackSource>
V4L2DmaTrackSource::Create(std::shared_ptr<VideoCapturer> capturer) {
    auto obj = rtc::make_ref_counted<V4L2DmaTrackSource>(std::move(capturer));
    obj->StartTrack();
    return obj;
}

V4L2DmaTrackSource::V4L2DmaTrackSource(std::shared_ptr<VideoCapturer> capturer)
    : ScaleTrackSource(capturer),
      is_dma_src_(capturer->is_dma_capture()),
      config_width_(capturer->width()),
      config_height_(capturer->height()) {}

V4L2DmaTrackSource::~V4L2DmaTrackSource() { scaler.reset(); }

void V4L2DmaTrackSource::StartTrack() {
    auto observer = capturer->AsFrameBufferObservable();
    observer->Subscribe([this](rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
        OnFrameCaptured(frame_buffer);
    });
}

void V4L2DmaTrackSource::OnFrameCaptured(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    const int64_t timestamp_us = rtc::TimeMicros();
    const int64_t translated_timestamp_us =
        timestamp_aligner.TranslateTimestamp(timestamp_us, rtc::TimeMicros());

    if (capturer->config().no_adaptive) {
        OnFrame(webrtc::VideoFrame::Builder()
                    .set_video_frame_buffer(frame_buffer)
                    .set_rotation(webrtc::kVideoRotation_0)
                    .set_timestamp_us(translated_timestamp_us)
                    .build());
    } else {
        int adapted_width, adapted_height, crop_width, crop_height, crop_x, crop_y;
        if (!AdaptFrame(width, height, timestamp_us, &adapted_width, &adapted_height, &crop_width,
                        &crop_height, &crop_x, &crop_y)) {
            return;
        }

        auto decoded_buffer = frame_buffer->GetRawBuffer();

        if (!scaler || adapted_width != config_width_ || adapted_height != config_height_) {
            config_width_ = adapted_width;
            config_height_ = adapted_height;
            scaler = V4L2Scaler::Create(width, height, decoded_buffer.pix_fmt, config_width_,
                                        config_height_, is_dma_src_, true);
        }

        scaler->EmplaceBuffer(decoded_buffer, [this,
                                               translated_timestamp_us](V4L2Buffer scaled_buffer) {
            auto dst_buffer = V4L2FrameBuffer::Create(config_width_, config_height_, scaled_buffer);

            OnFrame(webrtc::VideoFrame::Builder()
                        .set_video_frame_buffer(dst_buffer)
                        .set_rotation(webrtc::kVideoRotation_0)
                        .set_timestamp_us(translated_timestamp_us)
                        .build());
        });
    }
}
