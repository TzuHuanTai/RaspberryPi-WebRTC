#ifndef COMMON_JPEG_UTIL_H_
#define COMMON_JPEG_UTIL_H_

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

namespace jpeg_util {

struct FreeDeleter {
    void operator()(uint8_t *ptr) const {
        if (ptr) {
            free(ptr);
        }
    }
};

/* An owned, malloc-allocated JPEG blob, as produced by libjpeg's memory destination. */
struct JpegBuffer {
    std::unique_ptr<uint8_t, FreeDeleter> start;
    unsigned long length;
};

JpegBuffer ConvertYuvToJpeg(const uint8_t *yuv_data, int width, int height, int quality = 100);
void CreateJpegImage(const uint8_t *yuv_data, int width, int height, const std::string &url,
                     int quality);

} // namespace jpeg_util

#endif // COMMON_JPEG_UTIL_H_
