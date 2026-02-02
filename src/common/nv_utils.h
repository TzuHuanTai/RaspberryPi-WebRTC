#ifndef NV_UTILS_
#define NV_UTILS_

#include <cstddef>
#include <cstdint>

class NvUtils {
  public:
    static int ConvertToI420(int src_dma_fd, uint8_t *dst_addr, size_t dst_size, int width,
                             int height);
    static int ReadDmaBuffer(int src_dma_fd, uint8_t *dst_addr, size_t dst_size);
};

#endif // NV_UTILS_
