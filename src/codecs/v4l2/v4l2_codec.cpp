#include "codecs/v4l2/v4l2_codec.h"
#include "common/logging.h"
#include <cstring>
#include <thread>

V4L2Codec::V4L2Codec()
    : fd_(0),
      dst_fmt_(0),
      abort_(false) {}

V4L2Codec::~V4L2Codec() {
    abort_ = true;
    worker_.reset();
    V4L2Util::StreamOff(fd_, output_.type);
    V4L2Util::StreamOff(fd_, capture_.type);

    V4L2Util::DeallocateBuffer(fd_, &output_);
    V4L2Util::DeallocateBuffer(fd_, &capture_);

    V4L2Util::CloseDevice(fd_);
}

bool V4L2Codec::Open(const char *file_name) {
    file_name_ = file_name;
    fd_ = V4L2Util::OpenDevice(file_name);
    if (fd_ < 0) {
        return false;
    }
    return true;
}

bool V4L2Codec::PrepareBuffer(V4L2BufferGroup *gbuffer, int width, int height, uint32_t pix_fmt,
                              v4l2_buf_type type, v4l2_memory memory, int buffer_num,
                              bool has_dmafd) {
    if (!V4L2Util::InitBuffer(fd_, gbuffer, type, memory, has_dmafd)) {
        return false;
    }

    if (!V4L2Util::SetFormat(fd_, gbuffer, width, height, pix_fmt)) {
        return false;
    }

    if (!V4L2Util::AllocateBuffer(fd_, gbuffer, buffer_num)) {
        return false;
    }

    if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
        for (int i = 0; i < buffer_num; i++) {
            output_buffer_index_.push(i);
        }
    } else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        dst_fmt_ = pix_fmt;
        if (!V4L2Util::QueueBuffers(fd_, gbuffer)) {
            return false;
        }
    }

    return true;
}

void V4L2Codec::Start() {
    if (dst_fmt_ == 0) {
        ERROR_PRINT("The target format is not set.");
        exit(EXIT_FAILURE);
    }

    abort_ = false;
    worker_ = std::make_unique<Worker>(file_name_ + std::to_string(pthread_self()), [this]() {
        CaptureBuffer();
    });
    worker_->Run();
}

void V4L2Codec::EmplaceBuffer(V4L2Buffer &buffer, std::function<void(V4L2Buffer &)> on_capture) {
    auto item = output_buffer_index_.pop();
    if (!item) {
        return;
    }
    auto index = item.value();

    if (output_.memory == V4L2_MEMORY_DMABUF) {
        v4l2_buffer *buf = &output_.buffers[index].inner;
        buf->m.planes[0].m.fd = buffer.dmafd;
        buf->m.planes[0].bytesused = buffer.length;
        buf->m.planes[0].length = buffer.length;
    } else {
        memcpy((uint8_t *)output_.buffers[index].start, (uint8_t *)buffer.start, buffer.length);
    }

    if (!V4L2Util::QueueBuffer(fd_, &output_.buffers[index].inner)) {
        ERROR_PRINT("QueueBuffer V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE. fd(%d) at index %d", fd_,
                    index);
        output_buffer_index_.push(index);
        return;
    }

    capturing_tasks_.push(on_capture);
}

bool V4L2Codec::CaptureBuffer() {

    if (abort_) {
        return false;
    }

    fd_set fds[2];
    fd_set *rd_fds = &fds[0]; /* for capture */
    fd_set *ex_fds = &fds[1]; /* for handle event */
    FD_ZERO(rd_fds);
    FD_SET(fd_, rd_fds);
    FD_ZERO(ex_fds);
    FD_SET(fd_, ex_fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    int r = select(fd_ + 1, rd_fds, NULL, ex_fds, &tv);

    if (abort_) {
        return false;
    } else if (r <= 0) { // failed or timeout
        return false;
    }

    if (rd_fds && FD_ISSET(fd_, rd_fds)) {
        struct v4l2_buffer buf = {0};
        struct v4l2_plane planes = {0};
        buf.memory = output_.memory;
        buf.length = 1;
        buf.m.planes = &planes;
        buf.type = output_.type;
        if (!V4L2Util::DequeueBuffer(fd_, &buf)) {
            return false;
        }
        output_buffer_index_.push(buf.index);

        buf = {};
        planes = {};
        buf.memory = capture_.memory;
        buf.length = 1;
        buf.m.planes = &planes;
        buf.type = capture_.type;
        if (!V4L2Util::DequeueBuffer(fd_, &buf)) {
            return false;
        }

        auto buffer = V4L2Buffer::FromCapturedPlane(
            capture_.buffers[buf.index].start, buf.m.planes[0].bytesused,
            capture_.buffers[buf.index].dmafd, buf.flags, dst_fmt_);

        if (abort_) {
            return false;
        }

        auto item = capturing_tasks_.pop();
        if (item) {
            auto task = item.value();
            task(buffer);
        }

        if (!V4L2Util::QueueBuffer(fd_, &capture_.buffers[buf.index].inner)) {
            return false;
        }
    }

    if (ex_fds && FD_ISSET(fd_, ex_fds)) {
        ERROR_PRINT("Exception in fd(%d).", fd_);
        HandleEvent();
    }

    return true;
}
