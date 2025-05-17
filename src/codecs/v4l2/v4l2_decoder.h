#ifndef V4L2_DECODER_H_
#define V4L2_DECODER_H_

#include "codecs/v4l2/v4l2_codec.h"

class V4L2Decoder : public V4L2Codec {
  public:
    static std::unique_ptr<V4L2Decoder> Create(int width, int height, uint32_t src_pix_fmt,
                                               bool is_dma_dst);

  protected:
    bool Configure(int width, int height, uint32_t src_pix_fmt, bool is_dma_dst);
    void HandleEvent() override;

  private:
    uint32_t capture_format_ = V4L2_PIX_FMT_YUV420;
};

#endif // V4L2_DECODER_H_
