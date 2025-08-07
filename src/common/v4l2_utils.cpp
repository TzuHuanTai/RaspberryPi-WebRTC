#include "v4l2_utils.h"
#include "common/logging.h"

#include <errno.h>
#include <fcntl.h>
#include <stdexcept>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

bool V4L2Util::IsSinglePlaneVideo(v4l2_capability *cap) {
    return (cap->capabilities & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_OUTPUT) &&
            (cap->capabilities & V4L2_CAP_STREAMING)) ||
           (cap->capabilities & V4L2_CAP_VIDEO_M2M);
}

bool V4L2Util::IsMultiPlaneVideo(v4l2_capability *cap) {
    return (cap->capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE) &&
            (cap->capabilities & V4L2_CAP_STREAMING)) ||
           (cap->capabilities & V4L2_CAP_VIDEO_M2M_MPLANE);
}

std::string V4L2Util::FourccToString(uint32_t fourcc) {
    int length = 4;
    std::string buf;
    buf.resize(length);

    for (int i = 0; i < length; i++) {
        const int c = fourcc & 0xff;
        buf[i] = c;
        fourcc >>= 8;
    }
    return buf;
}

int V4L2Util::OpenDevice(const char *file) {
    int fd = open(file, O_RDWR);
    if (fd < 0) {
        throw std::runtime_error(std::string("Failed to open v4l2 device: ") + file);
    }
    DEBUG_PRINT("Successfully opened file %s (fd: %d)", file, fd);
    return fd;
}

void V4L2Util::CloseDevice(int fd) {
    close(fd);
    DEBUG_PRINT("fd(%d) is closed!", fd);
}

bool V4L2Util::QueryCapabilities(int fd, v4l2_capability *cap) {
    if (ioctl(fd, VIDIOC_QUERYCAP, cap) < 0) {
        ERROR_PRINT("fd(%d) query capabilities: %s", fd, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::InitBuffer(int fd, V4L2BufferGroup *gbuffer, v4l2_buf_type type, v4l2_memory memory,
                          bool has_dmafd) {
    v4l2_capability cap = {};
    if (!V4L2Util::QueryCapabilities(fd, &cap)) {
        return false;
    }

    DEBUG_PRINT("fd(%d) driver '%s' on card '%s' in %s mode", fd, cap.driver, cap.card,
                V4L2Util::IsSinglePlaneVideo(&cap)  ? "splane"
                : V4L2Util::IsMultiPlaneVideo(&cap) ? "mplane"
                                                    : "unknown");
    gbuffer->fd = fd;
    gbuffer->type = type;
    gbuffer->memory = memory;
    gbuffer->has_dmafd = has_dmafd;

    return true;
}

bool V4L2Util::DequeueBuffer(int fd, v4l2_buffer *buffer) {
    if (ioctl(fd, VIDIOC_DQBUF, buffer) < 0) {
        ERROR_PRINT("fd(%d) dequeue buffer: %s", fd, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::QueueBuffer(int fd, v4l2_buffer *buffer) {
    if (ioctl(fd, VIDIOC_QBUF, buffer) < 0) {
        ERROR_PRINT("fd(%d) queue buffer(%u): %s\n", fd, buffer->type, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::QueueBuffers(int fd, V4L2BufferGroup *gbuffer) {
    for (int i = 0; i < gbuffer->num_buffers; i++) {
        v4l2_buffer *inner = &gbuffer->buffers[i].inner;
        if (!V4L2Util::QueueBuffer(fd, inner)) {
            return false;
        }
    }
    return true;
}

std::unordered_set<std::string> V4L2Util::GetDeviceSupportedFormats(const char *file) {
    int fd = V4L2Util::OpenDevice(file);
    v4l2_fmtdesc fmtdesc = {0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    std::unordered_set<std::string> formats;

    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        auto pixel_format = V4L2Util::FourccToString(fmtdesc.pixelformat);
        formats.insert(pixel_format);
        fmtdesc.index++;
    }
    V4L2Util::CloseDevice(fd);

    return formats;
}

bool V4L2Util::SubscribeEvent(int fd, uint32_t type) {
    v4l2_event_subscription sub = {};
    sub.type = type;
    if (ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0) {
        ERROR_PRINT("fd(%d) does not support VIDIOC_SUBSCRIBE_EVENT(%d)", fd, type);
        return false;
    }
    return true;
}

bool V4L2Util::SetFps(int fd, v4l2_buf_type type, uint32_t fps) {
    struct v4l2_streamparm streamparms = {};
    streamparms.type = type;
    streamparms.parm.capture.timeperframe.numerator = 1;
    streamparms.parm.capture.timeperframe.denominator = fps;
    if (ioctl(fd, VIDIOC_S_PARM, &streamparms) < 0) {
        ERROR_PRINT("fd(%d) set fps(%d): %s", fd, fps, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::SetFormat(int fd, V4L2BufferGroup *gbuffer, uint32_t width, uint32_t height,
                         uint32_t &pixel_format) {
    v4l2_format fmt = {};
    fmt.type = gbuffer->type;
    ioctl(fd, VIDIOC_G_FMT, &fmt);

    DEBUG_PRINT("fd(%d) original formats: %s(%dx%d)", gbuffer->fd,
                V4L2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(), fmt.fmt.pix_mp.width,
                fmt.fmt.pix_mp.height);

    if (width > 0 && height > 0) {
        fmt.fmt.pix_mp.width = width;
        fmt.fmt.pix_mp.height = height;
        fmt.fmt.pix_mp.pixelformat = pixel_format;
    }

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        ERROR_PRINT("fd(%d) set format(%s) : %s", fd,
                    V4L2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(), strerror(errno));
        return false;
    }

    DEBUG_PRINT("fd(%d) latest format: %s(%dx%d)", gbuffer->fd,
                V4L2Util::FourccToString(fmt.fmt.pix_mp.pixelformat).c_str(), fmt.fmt.pix_mp.width,
                fmt.fmt.pix_mp.height);
    // use the  return format
    pixel_format = fmt.fmt.pix_mp.pixelformat;
    gbuffer->num_planes = fmt.fmt.pix_mp.num_planes;

    if (fmt.fmt.pix_mp.width != width || fmt.fmt.pix_mp.height != height) {
        ERROR_PRINT("fd(%d) input size (%dx%d) doesn't match driver's output size (%dx%d): %s", fd,
                    width, height, fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, strerror(EINVAL));
        throw std::runtime_error("the frame size doesn't match");
    }

    return true;
}

bool V4L2Util::SetCtrl(int fd, uint32_t id, int32_t value) {
    v4l2_control ctrls = {};
    ctrls.id = id;
    ctrls.value = value;
    if (ioctl(fd, VIDIOC_S_CTRL, &ctrls) < 0) {
        ERROR_PRINT("fd(%d) set ctrl(%d): %s", fd, id, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::SetExtCtrl(int fd, uint32_t id, int32_t value) {
    v4l2_ext_controls ctrls = {};
    v4l2_ext_control ctrl = {};

    /* set ctrls */
    ctrls.ctrl_class = V4L2_CTRL_CLASS_CODEC;
    ctrls.controls = &ctrl;
    ctrls.count = 1;

    /* set ctrl*/
    ctrl.id = id;
    ctrl.value = value;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0) {
        ERROR_PRINT("fd(%d) set ext ctrl(%d): %s", fd, id, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::StreamOn(int fd, v4l2_buf_type type) {
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        ERROR_PRINT("fd(%d) turn on stream: %s", fd, strerror(errno));
        return false;
    }
    return true;
}

bool V4L2Util::StreamOff(int fd, v4l2_buf_type type) {
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        ERROR_PRINT("fd(%d) turn off stream: %s", fd, strerror(errno));
        return false;
    }
    return true;
}

void V4L2Util::UnMap(V4L2BufferGroup *gbuffer) {
    for (int i = 0; i < gbuffer->num_buffers; i++) {
        if (gbuffer->buffers[i].dmafd != 0) {
            DEBUG_PRINT("close (%d) dmafd", gbuffer->buffers[i].dmafd);
            close(gbuffer->buffers[i].dmafd);
        }
        if (gbuffer->buffers[i].start != nullptr) {
            DEBUG_PRINT("unmapped (%d) buffers", gbuffer->fd);
            munmap(gbuffer->buffers[i].start, gbuffer->buffers[i].length);
            gbuffer->buffers[i].start = nullptr;
        }
    }
}

bool V4L2Util::MMap(int fd, V4L2BufferGroup *gbuffer) {
    for (int i = 0; i < gbuffer->num_buffers; i++) {
        V4L2Buffer *buffer = &gbuffer->buffers[i];
        v4l2_buffer *inner = &buffer->inner;
        inner->type = gbuffer->type;
        inner->memory = V4L2_MEMORY_MMAP;
        inner->length = 1;
        inner->index = i;
        inner->m.planes = buffer->plane;

        if (ioctl(fd, VIDIOC_QUERYBUF, inner) < 0) {
            ERROR_PRINT("fd(%d) query buffer: %s", fd, strerror(errno));
            return false;
        }

        if (gbuffer->has_dmafd) {
            v4l2_exportbuffer expbuf = {};
            expbuf.type = gbuffer->type;
            expbuf.index = i;
            expbuf.plane = 0;
            if (ioctl(fd, VIDIOC_EXPBUF, &expbuf) < 0) {
                ERROR_PRINT("fd(%d) export buffer: %s", fd, strerror(errno));
                return false;
            }
            buffer->dmafd = expbuf.fd;
            DEBUG_PRINT("fd(%d) export dma at fd(%d)", gbuffer->fd, buffer->dmafd);
        }

        if (gbuffer->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
            gbuffer->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            buffer->length = inner->m.planes[0].length;
            buffer->start = mmap(NULL, buffer->length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                 inner->m.planes[0].m.mem_offset);
        } else if (gbuffer->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
                   gbuffer->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
            buffer->length = inner->length;
            buffer->start =
                mmap(NULL, buffer->length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, inner->m.offset);
        }

        if (MAP_FAILED == buffer->start) {
            perror("MAP FAILED");
            munmap(buffer->start, buffer->length);
            buffer->start = nullptr;
            return false;
        }

        DEBUG_PRINT("fd(%d) query buffer at %p (length: %d)", gbuffer->fd, buffer->start,
                    buffer->length);
    }

    return true;
}

bool V4L2Util::AllocateBuffer(int fd, V4L2BufferGroup *gbuffer, int num_buffers) {
    gbuffer->num_buffers = num_buffers;
    gbuffer->buffers.resize(num_buffers);

    v4l2_requestbuffers req = {};
    req.count = num_buffers;
    req.memory = gbuffer->memory;
    req.type = gbuffer->type;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        ERROR_PRINT("fd(%d) request buffer: %s", fd, strerror(errno));
        return false;
    }

    if (gbuffer->memory == V4L2_MEMORY_MMAP) {
        return MMap(fd, gbuffer);
    } else if (gbuffer->memory == V4L2_MEMORY_DMABUF) {
        for (int i = 0; i < num_buffers; i++) {
            V4L2Buffer *buffer = &gbuffer->buffers[i];
            v4l2_buffer *inner = &buffer->inner;
            inner->type = gbuffer->type;
            inner->memory = V4L2_MEMORY_DMABUF;
            inner->index = i;
            inner->length = 1;
            inner->m.planes = buffer->plane;
        }
    }

    return true;
}

bool V4L2Util::DeallocateBuffer(int fd, V4L2BufferGroup *gbuffer) {
    if (gbuffer->memory == V4L2_MEMORY_MMAP) {
        V4L2Util::UnMap(gbuffer);
    }

    v4l2_requestbuffers req = {};
    req.count = 0;
    req.memory = gbuffer->memory;
    req.type = gbuffer->type;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        ERROR_PRINT("fd(%d) request buffer: %s", fd, strerror(errno));
        return false;
    }

    gbuffer->fd = 0;
    gbuffer->has_dmafd = false;

    return true;
}
