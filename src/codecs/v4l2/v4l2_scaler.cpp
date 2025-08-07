#include "codecs/v4l2/v4l2_scaler.h"
#include "common/logging.h"

const char *SCALER_FILE = "/dev/video12";
const int BUFFER_NUM = 2;

std::unique_ptr<V4L2Scaler> V4L2Scaler::Create(int src_width, int src_height, uint32_t src_pix_fmt,
                                               int dst_width, int dst_height, bool is_dma_src,
                                               bool is_dma_dst) {
    auto scaler = std::make_unique<V4L2Scaler>();
    scaler->Configure(src_width, src_height, src_pix_fmt, dst_width, dst_height, is_dma_src,
                      is_dma_dst);
    scaler->Start();
    return scaler;
}

void V4L2Scaler::Configure(int src_width, int src_height, uint32_t src_pix_fmt, int dst_width,
                           int dst_height, bool is_dma_src, bool is_dma_dst) {
    if (!Open(SCALER_FILE)) {
        ERROR_PRINT("Unable to turn on scaler: %s", SCALER_FILE);
    }

    auto src_memory = is_dma_src ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
    if (!SetupOutputBuffer(src_width, src_height, src_pix_fmt, src_memory, BUFFER_NUM)) {
        ERROR_PRINT("Could not setup output buffer");
    }
    if (!SetupCaptureBuffer(dst_width, dst_height, V4L2_PIX_FMT_YUV420, V4L2_MEMORY_MMAP,
                            BUFFER_NUM, is_dma_dst)) {
        ERROR_PRINT("Could not setup capture buffer");
    }
}
