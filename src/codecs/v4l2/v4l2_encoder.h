#ifndef V4L2_ENCODER_H_
#define V4L2_ENCODER_H_

#include "codecs/v4l2/v4l2_codec.h"

#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>

class V4L2Encoder : public V4L2Codec {
  public:
    static std::unique_ptr<V4L2Encoder> Create(int width, int height, uint32_t src_pix_fmt,
                                               bool is_dma_src);
    V4L2Encoder();

    void ForceKeyFrame();
    void SetLevel(uint32_t level);
    void SetProfile(uint32_t profile);
    void SetFps(uint32_t adjusted_fps);
    void SetRateControlMode(uint32_t mode);
    void SetIFrameInterval(uint32_t interval);
    void SetBitrate(uint32_t adjusted_bitrate_bps);

  private:
    int framerate_;
    int bitrate_bps_;

    void Configure(int width, int height, uint32_t src_pix_fmt, bool is_dma_src);
};

#endif // V4L2_ENCODER_H_
