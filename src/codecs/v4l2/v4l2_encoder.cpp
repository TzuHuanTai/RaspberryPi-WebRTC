#include "codecs/v4l2/v4l2_encoder.h"
#include "common/logging.h"

const char *ENCODER_FILE = "/dev/video11";
const int BUFFER_NUM = 2;
const int KEY_FRAME_INTERVAL = 600;

std::unique_ptr<V4L2Encoder> V4L2Encoder::Create(int width, int height, uint32_t src_pix_fmt,
                                                 bool is_dma_src) {
    auto encoder = std::make_unique<V4L2Encoder>();
    encoder->Configure(width, height, src_pix_fmt, is_dma_src);
    encoder->Start();
    return encoder;
}

V4L2Encoder::V4L2Encoder()
    : V4L2Codec(),
      framerate_(30),
      bitrate_bps_(10000000) {}

bool V4L2Encoder::Configure(int width, int height, uint32_t src_pix_fmt, bool is_dma_src) {
    if (!Open(ENCODER_FILE)) {
        DEBUG_PRINT("Failed to turn on encoder: %s", ENCODER_FILE);
        return false;
    }

    V4L2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true);
    V4L2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_PROFILE,
                         V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
    V4L2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
    V4L2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, KEY_FRAME_INTERVAL);
    V4L2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_);

    auto src_memory = is_dma_src ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
    PrepareBuffer(&output_, width, height, src_pix_fmt, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
                  src_memory, BUFFER_NUM);
    PrepareBuffer(&capture_, width, height, V4L2_PIX_FMT_H264, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
                  V4L2_MEMORY_MMAP, BUFFER_NUM);

    V4L2Util::StreamOn(fd_, output_.type);
    V4L2Util::StreamOn(fd_, capture_.type);

    return true;
}

void V4L2Encoder::SetBitrate(uint32_t adjusted_bitrate_bps) {
    if (adjusted_bitrate_bps < 1000000) {
        adjusted_bitrate_bps = 1000000;
    } else {
        adjusted_bitrate_bps = (adjusted_bitrate_bps / 25000) * 25000;
    }

    if (bitrate_bps_ != adjusted_bitrate_bps) {
        bitrate_bps_ = adjusted_bitrate_bps;
        if (!V4L2Util::SetExtCtrl(fd_, V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_)) {
            DEBUG_PRINT("Failed to set bitrate: %d bps", bitrate_bps_);
        }
    }
}

void V4L2Encoder::SetFps(int adjusted_fps) {
    if (framerate_ != adjusted_fps) {
        framerate_ = adjusted_fps;
        if (!V4L2Util::SetFps(fd_, output_.type, framerate_)) {
            DEBUG_PRINT("Failed to set output fps: %d", framerate_);
        }
    }
}

const int V4L2Encoder::GetFd() const { return fd_; }
