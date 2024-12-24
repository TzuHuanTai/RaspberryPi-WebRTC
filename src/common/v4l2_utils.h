#ifndef V4L2_UTILS_
#define V4L2_UTILS_

#include <linux/videodev2.h>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

struct V4l2Buffer {
    void *start = nullptr;
    unsigned int length;
    unsigned int flags = 0;
    int dmafd = 0;
    struct timeval timestamp = {0, 0};
    struct v4l2_buffer inner;
    struct v4l2_plane plane;

    V4l2Buffer() = default;
    V4l2Buffer(void *start, unsigned int length)
        : start(start),
          length(length) {}
    V4l2Buffer(void *start, unsigned int length, unsigned int flags, struct timeval timestamp)
        : start(start),
          length(length),
          flags(flags),
          timestamp(timestamp) {}
    ~V4l2Buffer() = default;
};

struct V4l2BufferGroup {
    int fd = 0;
    int num_buffers = 0;
    bool has_dmafd = false;
    std::vector<V4l2Buffer> buffers;
    enum v4l2_buf_type type;
    enum v4l2_memory memory;
};

class V4l2Util {
  public:
    static bool IsSinglePlaneVideo(v4l2_capability *cap);
    static bool IsMultiPlaneVideo(v4l2_capability *cap);
    static std::string FourccToString(uint32_t fourcc);

    static int OpenDevice(const char *file);
    static void CloseDevice(int fd);
    static bool QueryCapabilities(int fd, v4l2_capability *cap);
    static bool InitBuffer(int fd, V4l2BufferGroup *gbuffer, v4l2_buf_type type, v4l2_memory memory,
                           bool has_dmafd = false);
    static bool DequeueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffers(int fd, V4l2BufferGroup *buffer);
    static std::unordered_set<std::string> GetDeviceSupportedFormats(const char *file);
    static bool SubscribeEvent(int fd, uint32_t type);
    static bool SetFps(int fd, v4l2_buf_type type, int fps);
    static bool SetFormat(int fd, V4l2BufferGroup *gbuffer, int width, int height,
                          uint32_t pixel_format);
    static bool SetCtrl(int fd, uint32_t id, int32_t value);
    static bool SetExtCtrl(int fd, uint32_t id, int32_t value);
    static bool StreamOn(int fd, v4l2_buf_type type);
    static bool StreamOff(int fd, v4l2_buf_type type);
    static void UnMap(V4l2BufferGroup *gbuffer);
    static bool MMap(int fd, V4l2BufferGroup *gbuffer);
    static bool AllocateBuffer(int fd, V4l2BufferGroup *gbuffer, int num_buffers);
    static bool DeallocateBuffer(int fd, V4l2BufferGroup *gbuffer);
};

#endif // V4L2_UTILS_
