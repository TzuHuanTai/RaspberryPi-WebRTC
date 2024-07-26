#include "v4l2_codecs/v4l2_decoder.h"

#include <sys/ioctl.h>

#include "common/logging.h"
#include "v4l2_codecs/raw_buffer.h"

const char *DECODER_FILE = "/dev/video10";
const int BUFFER_NUM = 2;

bool V4l2Decoder::Configure(int width, int height, uint32_t src_pix_fmt, bool is_drm_dst) {
    if (!Open(DECODER_FILE)) {
        DEBUG_PRINT("Failed to turn on decoder: %s", DECODER_FILE);
        return false;
    }

    PrepareBuffer(&output_, width, height, src_pix_fmt, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                  V4L2_MEMORY_MMAP, BUFFER_NUM);
    PrepareBuffer(&capture_, width, height, V4L2_PIX_FMT_YUV420, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                  V4L2_MEMORY_MMAP, BUFFER_NUM, is_drm_dst);

    V4l2Util::SubscribeEvent(fd_, V4L2_EVENT_SOURCE_CHANGE);
    V4l2Util::SubscribeEvent(fd_, V4L2_EVENT_EOS);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    return true;
}

void V4l2Decoder::HandleEvent() {
    struct v4l2_event ev;
    while (!ioctl(fd_, VIDIOC_DQEVENT, &ev)) {
        switch (ev.type) {
            case V4L2_EVENT_SOURCE_CHANGE:
                DEBUG_PRINT("Source changed!");
                V4l2Util::StreamOff(fd_, capture_.type);
                V4l2Util::DeallocateBuffer(fd_, &capture_);
                V4l2Util::SetFormat(fd_, &capture_, 0, 0, 0);
                V4l2Util::AllocateBuffer(fd_, &capture_, BUFFER_NUM);
                V4l2Util::StreamOn(fd_, capture_.type);
                break;
            case V4L2_EVENT_EOS:
                DEBUG_PRINT("EOS!");
                break;
        }
    }
}
