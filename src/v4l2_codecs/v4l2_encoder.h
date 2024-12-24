#ifndef V4L2_ENCODER_H_
#define V4L2_ENCODER_H_

#include "v4l2_codecs/v4l2_codec.h"

#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>

class V4l2Encoder : public V4l2Codec {
  public:
    V4l2Encoder();

    bool Configure(int width, int height, bool is_dma_src);
    void SetProfile(uint32_t h264_profile);
    void SetBitrate(uint32_t adjusted_bitrate_bps);
    void SetFps(int adjusted_fps);
    const int GetFd() const;

  private:
    int framerate_;
    int bitrate_bps_;
    uint32_t h264_profile_;
};

#endif // V4L2_ENCODER_H_
