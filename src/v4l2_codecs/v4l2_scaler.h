#ifndef V4L2_SCALER_H_
#define V4L2_SCALER_H_

#include "v4l2_codecs/v4l2_codec.h"

class V4l2Scaler : public V4l2Codec {
  public:
    bool Configure(int src_width, int src_height, int dst_width, int dst_height, bool is_drm_src,
                   bool is_drm_dst);
};

#endif // V4L2_SCALER_H_
