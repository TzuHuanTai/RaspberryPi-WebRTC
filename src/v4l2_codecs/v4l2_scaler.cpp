#include "v4l2_codecs/v4l2_scaler.h"
#include "common/logging.h"

const char *SCALER_FILE = "/dev/video12";
const int BUFFER_NUM = 2;

std::unique_ptr<V4l2Scaler> V4l2Scaler::Create(int src_width, int src_height, int dst_width,
                                               int dst_height, bool is_dma_src, bool is_dma_dst) {
    auto scaler = std::make_unique<V4l2Scaler>();
    scaler->Configure(src_width, src_height, dst_width, dst_height, is_dma_src, is_dma_dst);
    scaler->Start();
    return scaler;
}

bool V4l2Scaler::Configure(int src_width, int src_height, int dst_width, int dst_height,
                           bool is_dma_src, bool is_dma_dst) {
    if (!Open(SCALER_FILE)) {
        DEBUG_PRINT("Failed to turn on scaler: %s", SCALER_FILE);
        return false;
    }
    auto src_memory = is_dma_src ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
    PrepareBuffer(&output_, src_width, src_height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, src_memory, BUFFER_NUM);
    PrepareBuffer(&capture_, dst_width, dst_height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM, is_dma_dst);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    return true;
}
