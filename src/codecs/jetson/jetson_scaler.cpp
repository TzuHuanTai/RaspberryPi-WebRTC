#include "codecs/jetson/jetson_scaler.h"
#include "common/logging.h"

#include <chrono>

std::unique_ptr<JetsonScaler> JetsonScaler::Create(int src_width, int src_height, int dst_width,
                                                   int dst_height) {
    auto scaler = std::make_unique<JetsonScaler>();
    if (scaler->Configure(src_width, src_height, dst_width, dst_height)) {
        return scaler;
    } else {
        return nullptr;
    }
}

JetsonScaler::JetsonScaler()
    : num_buffer_(1),
      current_buffer_index_(0) {}

JetsonScaler::~JetsonScaler() {
    for (auto fd : dst_dma_pool_) {
        int ret = NvBufSurf::NvDestroy(fd);
        if (ret < 0) {
            ERROR_PRINT("Failed to Destroy NvBuffer");
        }
    }

    DEBUG_PRINT("~JetsonScaler");
}

bool JetsonScaler::Configure(int src_width, int src_height, int dst_width, int dst_height) {
    dst_width_ = dst_width;
    dst_height_ = dst_height;

    memset(&transform_params_, 0, sizeof(transform_params_));
    transform_params_.src_top = 0;
    transform_params_.src_left = 0;
    transform_params_.src_width = src_width;
    transform_params_.src_height = src_height;
    transform_params_.dst_top = 0;
    transform_params_.dst_left = 0;
    transform_params_.dst_width = dst_width;
    transform_params_.dst_height = dst_height;
    transform_params_.flip = NvBufSurfTransform_None;
    transform_params_.filter = NvBufSurfTransformInter_Nearest;

    return PrepareScaledDmaBuffer(dst_width, dst_height);
}

bool JetsonScaler::PrepareScaledDmaBuffer(int width, int height) {
    dst_dma_pool_.clear();

    for (int i = 0; i < num_buffer_; ++i) {
        int dmafd;
        NvBufSurf::NvCommonAllocateParams cParams;
        cParams.width = width;
        cParams.height = height;
        cParams.layout = NVBUF_LAYOUT_PITCH;
        cParams.colorFormat = NVBUF_COLOR_FORMAT_YUV420;
        cParams.memtag = NvBufSurfaceTag_VIDEO_ENC;
        cParams.memType = NVBUF_MEM_SURFACE_ARRAY;

        if (NvBufSurf::NvAllocate(&cParams, 1, &dmafd) < 0)
            return false;

        dst_dma_pool_.push_back(dmafd);
    }

    return true;
}

void JetsonScaler::EmplaceBuffer(V4L2FrameBufferRef frame_buffer,
                                 std::function<void(V4L2FrameBufferRef)> on_capture) {

    auto dst_dma_fd = dst_dma_pool_[current_buffer_index_];

    int ret = NvBufSurf::NvTransform(&transform_params_, frame_buffer->GetDmaFd(), dst_dma_fd);
    if (ret < 0) {
        ERROR_PRINT("NvTransform failed to tranform from fd(%d) to fd(%d)",
                    frame_buffer->GetDmaFd(), dst_dma_fd);
    }

    auto v4l2_buffer =
        V4L2Buffer::FromCapturedPlane(nullptr, 0, dst_dma_fd, 0, V4L2_PIX_FMT_YUV420M);
    auto scaled_frame_buffer_ = V4L2FrameBuffer::Create(dst_width_, dst_height_, v4l2_buffer);

    on_capture(scaled_frame_buffer_);

    current_buffer_index_ = (current_buffer_index_ + 1) % num_buffer_;
}
