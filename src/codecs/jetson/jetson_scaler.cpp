#include "codecs/jetson/jetson_scaler.h"
#include "common/logging.h"

#include <chrono>

std::unique_ptr<JetsonScaler> JetsonScaler::Create(ScalerConfig config) {
    auto scaler = std::make_unique<JetsonScaler>(config);
    if (!scaler->Initialize()) {
        return nullptr;
    }
    scaler->Start();
    return scaler;
}

JetsonScaler::JetsonScaler(ScalerConfig config)
    : config_(config),
      num_buffer_(2),
      abort_(false),
      free_buffers_(num_buffer_),
      capturing_tasks_(num_buffer_) {}

JetsonScaler::~JetsonScaler() {
    abort_ = true;
    worker_.reset();

    while (auto task = capturing_tasks_.pop()) {
        // Return the unused dma fd back to free buffers
        free_buffers_.push(task->dst_dma_fd);
    }

    while (auto item = free_buffers_.pop()) {
        int fd = item.value();
        int ret = NvBufSurf::NvDestroy(fd);
        if (ret < 0) {
            ERROR_PRINT("Failed to Destroy NvBuffer");
        }
    }

    DEBUG_PRINT("~JetsonScaler");
}

bool JetsonScaler::Initialize() {
    memset(&transform_params_, 0, sizeof(transform_params_));
    transform_params_.src_top = 0;
    transform_params_.src_left = 0;
    transform_params_.src_width = config_.src_width;
    transform_params_.src_height = config_.src_height;
    transform_params_.dst_top = 0;
    transform_params_.dst_left = 0;
    transform_params_.dst_width = config_.dst_width;
    transform_params_.dst_height = config_.dst_height;
    transform_params_.flip = NvBufSurfTransform_None;
    transform_params_.filter = NvBufSurfTransformInter_Nearest;

    for (int i = 0; i < num_buffer_; ++i) {
        int dmafd;
        NvBufSurf::NvCommonAllocateParams cParams{};
        cParams.width = config_.dst_width;
        cParams.height = config_.dst_height;
        cParams.layout = NVBUF_LAYOUT_BLOCK_LINEAR;
        cParams.colorFormat = NVBUF_COLOR_FORMAT_NV12;
        cParams.memtag = NvBufSurfaceTag_VIDEO_ENC;
        cParams.memType = NVBUF_MEM_SURFACE_ARRAY;

        if (NvBufSurf::NvAllocate(&cParams, 1, &dmafd) < 0) {
            ERROR_PRINT("Failed to allocate NvBuffer");
            return false;
        }

        free_buffers_.push(dmafd);
    }

    return true;
}

void JetsonScaler::Start() {
    worker_ = std::make_unique<Worker>("NvTransform", [this]() {
        CaptureBuffer();
    });
    worker_->Run();
}

void JetsonScaler::EmplaceBuffer(V4L2FrameBufferRef frame_buffer,
                                 std::function<void(V4L2FrameBufferRef)> on_capture) {
    if (abort_) {
        return;
    }

    auto item = free_buffers_.pop();
    if (!item) {
        return;
    }

    int dst_dma_fd = item.value();

    int ret = NvBufSurf::NvTransform(&transform_params_, frame_buffer->GetDmaFd(), dst_dma_fd);
    if (ret < 0) {
        ERROR_PRINT("NvTransform failed to tranform from fd(%d) to fd(%d)",
                    frame_buffer->GetDmaFd(), dst_dma_fd);
        free_buffers_.push(dst_dma_fd);
        return;
    }

    CaptureTask task;
    task.dst_dma_fd = dst_dma_fd;
    task.callback = [this, dst_dma_fd, on_capture]() {
        auto v4l2_buffer =
            V4L2Buffer::FromCapturedPlane(nullptr, 0, dst_dma_fd, 0, V4L2_PIX_FMT_NV12);
        auto scaled_frame =
            V4L2FrameBuffer::Create(config_.dst_width, config_.dst_height, v4l2_buffer);
        on_capture(scaled_frame);
        free_buffers_.push(dst_dma_fd);
    };

    capturing_tasks_.push(std::move(task));
}

void JetsonScaler::CaptureBuffer() {
    if (auto task = capturing_tasks_.pop(1)) {
        task->callback();
    }
}
