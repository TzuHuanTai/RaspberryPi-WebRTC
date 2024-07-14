#ifndef V4L2_DECODER_H_
#define V4L2_DECODER_H_

#include "v4l2_codecs/v4l2_codec.h"

class V4l2Decoder : public V4l2Codec {
public:
    V4l2Decoder() {};
    ~V4l2Decoder() {};
    bool Configure(int width, int height, uint32_t src_pix_fmt, bool is_drm_dst);

protected:
    void HandleEvent() override;
};

#endif // V4L2_DECODER_H_
