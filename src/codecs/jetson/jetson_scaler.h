#ifndef JETSON_SCALER_H_
#define JETSON_SCALER_H_

#include <functional>
#include <memory>

#include "codecs/frame_processor.h"
#include "common/thread_safe_queue.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"

#include <NvBufSurface.h>

class JetsonScaler : public IFrameProcessor {
  public:
    struct CaptureTask {
        int dst_dma_fd;
        std::function<void()> callback;
    };

    static std::unique_ptr<JetsonScaler> Create(ScalerConfig config);

    JetsonScaler(ScalerConfig config);
    ~JetsonScaler() override;

    void EmplaceBuffer(V4L2FrameBufferRef buffer,
                       std::function<void(V4L2FrameBufferRef)> on_capture) override;

  protected:
    bool Initialize() override;
    void CaptureBuffer();
    void Start();

  private:
    ScalerConfig config_;
    int num_buffer_;
    std::atomic<bool> abort_;
    std::unique_ptr<Worker> worker_;
    NvBufSurf::NvCommonTransformParams transform_params_;
    ThreadSafeQueue<int> free_buffers_;
    ThreadSafeQueue<CaptureTask> capturing_tasks_;
};

#endif
