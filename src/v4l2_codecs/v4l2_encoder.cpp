#include "v4l2_codecs/v4l2_encoder.h"
#include "common/logging.h"

const char *ENCODER_FILE = "/dev/video11";
const int BUFFER_NUM = 4;
const int KEY_FRAME_INTERVAL = 240;

V4l2Encoder::V4l2Encoder()
    : framerate_(30),
      bitrate_bps_(10000000),
      h264_profile_(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) {}

bool V4l2Encoder::Configure(int width, int height, bool is_drm_src) {
    if (!Open(ENCODER_FILE)) {
        DEBUG_PRINT("Failed to turn on encoder: %s", ENCODER_FILE);
        return false;
    }

    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE, h264_profile_);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
    V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, KEY_FRAME_INTERVAL);

    auto src_memory = is_drm_src? V4L2_MEMORY_DMABUF: V4L2_MEMORY_MMAP;
    PrepareBuffer(&output_, width, height, V4L2_PIX_FMT_YUV420,
                  V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, src_memory, BUFFER_NUM);
    PrepareBuffer(&capture_, width, height, V4L2_PIX_FMT_H264,
                  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP, BUFFER_NUM);

    V4l2Util::StreamOn(fd_, output_.type);
    V4l2Util::StreamOn(fd_, capture_.type);

    return true;
}

void V4l2Encoder::SetProfile(uint32_t h264_profile) {
    h264_profile_ = h264_profile;
}

void V4l2Encoder::SetBitrate(uint32_t adjusted_bitrate_bps) {
    if (adjusted_bitrate_bps < 1000000) {
        adjusted_bitrate_bps = 1000000;
    } else {
        adjusted_bitrate_bps = (adjusted_bitrate_bps / 25000) * 25000;
    }

    if (bitrate_bps_ != adjusted_bitrate_bps) {
        bitrate_bps_ = adjusted_bitrate_bps;
        if (!V4l2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_)) {
            DEBUG_PRINT("Failed to set bitrate: %d bps", bitrate_bps_);
        }
    }
}

void V4l2Encoder::SetFps(int adjusted_fps) {
    if (framerate_ != adjusted_fps) {
        framerate_ = adjusted_fps;
        if (!V4l2Util::SetFps(fd_, output_.type, framerate_)) {
            DEBUG_PRINT("Failed to set output fps: %d", framerate_);
        }
    }
}

const int V4l2Encoder::GetFd() const {
    return fd_;
}
