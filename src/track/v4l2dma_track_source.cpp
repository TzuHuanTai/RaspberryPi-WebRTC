#include "track/v4l2dma_track_source.h"

#if defined(USE_RPI_HW_ENCODER)
#include "codecs/v4l2/v4l2_scaler.h"
#elif defined(USE_JETSON_HW_ENCODER)
#include "codecs/jetson/jetson_scaler.h"
#endif
#include "common/logging.h"

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
    observer->Subscribe([this](V4L2FrameBufferRef frame_buffer) {
        OnFrameCaptured(frame_buffer);
    });
}

void V4L2DmaTrackSource::OnFrameCaptured(V4L2FrameBufferRef frame_buffer) {
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

        if (!scaler || adapted_width != config_width_ || adapted_height != config_height_) {
            config_width_ = adapted_width;
            config_height_ = adapted_height;
#if defined(USE_RPI_HW_ENCODER)
            scaler = V4L2Scaler::Create(width, height, frame_buffer->format(), config_width_,
                                        config_height_, is_dma_src_, true);
#elif defined(USE_JETSON_HW_ENCODER)
            scaler = JetsonScaler::Create(width, height, config_width_, config_height_);
#endif
            DEBUG_PRINT("New scaler is set: %dx%d -> %dx%d", width, height, config_width_,
                        config_height_);
        }

        scaler->EmplaceBuffer(frame_buffer,
                              [this, translated_timestamp_us](V4L2FrameBufferRef scaled_buffer) {
                                  OnFrame(webrtc::VideoFrame::Builder()
                                              .set_video_frame_buffer(scaled_buffer)
                                              .set_rotation(webrtc::kVideoRotation_0)
                                              .set_timestamp_us(translated_timestamp_us)
                                              .build());
                              });
    }
}
