#ifndef V4L2DMA_TRACK_SOURCE_H_
#define V4L2DMA_TRACK_SOURCE_H_

#include "track/scale_track_source.h"
#include "v4l2_codecs/v4l2_scaler.h"

class V4l2DmaTrackSource : public ScaleTrackSource {
  public:
    static rtc::scoped_refptr<V4l2DmaTrackSource> Create(std::shared_ptr<VideoCapturer> capturer);
    V4l2DmaTrackSource(std::shared_ptr<VideoCapturer> capturer);
    ~V4l2DmaTrackSource();
    void StartTrack() override;

  private:
    bool is_dma_src_;
    int config_width_;
    int config_height_;
    std::unique_ptr<V4l2Scaler> scaler;

    void Init();
    void OnFrameCaptured(V4l2Buffer buffer);
};

#endif
