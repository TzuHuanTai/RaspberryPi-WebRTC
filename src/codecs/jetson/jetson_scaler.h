#ifndef JETSON_SCALER_H_
#define JETSON_SCALER_H_

#include <functional>
#include <memory>

#include "common/interface/processor.h"
#include "common/v4l2_utils.h"

#include <NvBufSurface.h>

class JetsonScaler : public IFrameProcessor {
  public:
    static std::unique_ptr<JetsonScaler> Create(int src_width, int src_height, int dst_width,
                                                int dst_height);

    JetsonScaler();
    ~JetsonScaler() override;

    void EmplaceBuffer(V4L2FrameBufferRef buffer,
                       std::function<void(V4L2FrameBufferRef)> on_capture) override;

  private:
    int dst_width_;
    int dst_height_;
    int num_buffer_;
    int current_buffer_index_;
    std::vector<int> dst_dma_pool_;
    NvBufSurf::NvCommonTransformParams transform_params_;

    bool Configure(int src_width, int src_height, int dst_width, int dst_height);
    bool PrepareScaledDmaBuffer(int width, int height);
};

#endif
