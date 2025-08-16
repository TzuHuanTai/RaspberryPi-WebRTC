#ifndef V4L2_UTILS_
#define V4L2_UTILS_

#include <linux/videodev2.h>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include <vector>

/* Save single-plane data with stride equal to width */
struct V4L2Buffer {
    void *start = nullptr;
    uint32_t pix_fmt;
    uint32_t length;
    uint32_t flags = 0;
    int dmafd = -1;
    struct timeval timestamp = {0, 0};
    struct v4l2_buffer inner;
    struct v4l2_plane plane[VIDEO_MAX_PLANES];

    V4L2Buffer() = default;

    static V4L2Buffer FromV4L2(void *start, const v4l2_buffer &v4l2, uint32_t fmt) {
        V4L2Buffer buf;
        buf.start = start;
        buf.pix_fmt = fmt;
        buf.flags = v4l2.flags;
        buf.length = v4l2.bytesused;
        buf.timestamp = v4l2.timestamp;
        buf.inner = v4l2;
        return buf;
    }

    static V4L2Buffer FromLibcamera(void *start, int length, int dmafd, timeval timestamp,
                                    uint32_t fmt) {
        V4L2Buffer buf;
        buf.start = start;
        buf.dmafd = dmafd;
        buf.pix_fmt = fmt;
        buf.length = length;
        buf.timestamp = timestamp;
        return buf;
    }

    static V4L2Buffer FromCapturedPlane(void *start, uint32_t bytesused, int dmafd, uint32_t flags,
                                        uint32_t pix_fmt) {
        V4L2Buffer buf;
        buf.start = start;
        buf.dmafd = dmafd;
        buf.pix_fmt = pix_fmt;
        buf.length = bytesused;
        buf.flags = flags;
        return buf;
    }
};

struct V4L2BufferGroup {
    int fd = -1;
    uint32_t num_planes = 0;
    uint32_t num_buffers = 0;
    bool has_dmafd = false;
    std::vector<V4L2Buffer> buffers;
    enum v4l2_buf_type type;
    enum v4l2_memory memory;
};

class V4L2Util {
  public:
    static bool IsSinglePlaneVideo(v4l2_capability *cap);
    static bool IsMultiPlaneVideo(v4l2_capability *cap);
    static std::string FourccToString(uint32_t fourcc);

    static int OpenDevice(const char *file);
    static void CloseDevice(int fd);
    static bool QueryCapabilities(int fd, v4l2_capability *cap);
    static bool InitBuffer(int fd, V4L2BufferGroup *gbuffer, v4l2_buf_type type, v4l2_memory memory,
                           bool has_dmafd = false);
    static bool DequeueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffer(int fd, v4l2_buffer *buffer);
    static bool QueueBuffers(int fd, V4L2BufferGroup *buffer);
    static std::unordered_set<std::string> GetDeviceSupportedFormats(const char *file);
    static bool SubscribeEvent(int fd, uint32_t type);
    static bool SetFps(int fd, v4l2_buf_type type, uint32_t fps);
    static bool SetFormat(int fd, V4L2BufferGroup *gbuffer, uint32_t width, uint32_t height,
                          uint32_t &pixel_format);
    static bool SetCtrl(int fd, uint32_t id, int32_t value);
    static bool SetExtCtrl(int fd, uint32_t id, int32_t value);
    static bool StreamOn(int fd, v4l2_buf_type type);
    static bool StreamOff(int fd, v4l2_buf_type type);
    static void UnMap(V4L2BufferGroup *gbuffer);
    static bool MMap(int fd, V4L2BufferGroup *gbuffer);
    static bool AllocateBuffer(int fd, V4L2BufferGroup *gbuffer, int num_buffers);
    static bool DeallocateBuffer(int fd, V4L2BufferGroup *gbuffer);
};

#endif // V4L2_UTILS_
