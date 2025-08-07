#include "codecs/v4l2/v4l2_decoder.h"
#include "common/logging.h"

const char *DECODER_FILE = "/dev/video10";
const int BUFFER_NUM = 2;

std::unique_ptr<V4L2Decoder> V4L2Decoder::Create(int width, int height, uint32_t src_pix_fmt,
                                                 bool is_dma_dst) {
    auto decoder = std::make_unique<V4L2Decoder>();
    decoder->Configure(width, height, src_pix_fmt, is_dma_dst);
    decoder->Start();
    return decoder;
}

void V4L2Decoder::Configure(int width, int height, uint32_t src_pix_fmt, bool is_dma_dst) {
    if (!Open(DECODER_FILE)) {
        ERROR_PRINT("Unable to turn on decoder: %s", DECODER_FILE);
    }

    if (!SetupOutputBuffer(width, height, src_pix_fmt, V4L2_MEMORY_MMAP, BUFFER_NUM)) {
        ERROR_PRINT("Could not setup output buffer");
    }
    if (!SetupCaptureBuffer(width, height, V4L2_PIX_FMT_YUV420, V4L2_MEMORY_MMAP, BUFFER_NUM,
                            is_dma_dst)) {
        ERROR_PRINT("Could not setup capture buffer");
    }

    if (!SubscribeEvent(V4L2_EVENT_SOURCE_CHANGE)) {
        ERROR_PRINT("Could not subscribe source change event");
    }
    if (!SubscribeEvent(V4L2_EVENT_EOS)) {
        ERROR_PRINT("Could not subscribe EOS event");
    }
}
