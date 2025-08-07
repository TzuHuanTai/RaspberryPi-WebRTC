#ifndef V4L2_SCALER_H_
#define V4L2_SCALER_H_

#include "codecs/v4l2/v4l2_codec.h"

class V4L2Scaler : public V4L2Codec {
  public:
    static std::unique_ptr<V4L2Scaler> Create(int src_width, int src_height, uint32_t src_pix_fmt,
                                              int dst_width, int dst_height, bool is_dma_src,
                                              bool is_dma_dst);

  private:
    void Configure(int src_width, int src_height, uint32_t src_pix_fmt, int dst_width,
                   int dst_height, bool is_drm_src, bool is_drm_dst);
};

#endif // V4L2_SCALER_H_
