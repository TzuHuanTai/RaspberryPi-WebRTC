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
      bitrate_bps_(2 * 1024 * 1024) {}

void V4L2Encoder::Configure(int width, int height, uint32_t src_pix_fmt, bool is_dma_src) {
    if (!Open(ENCODER_FILE)) {
        ERROR_PRINT("Unable to turn on encoder: %s", ENCODER_FILE);
    }

    SetProfile(V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
    SetLevel(V4L2_MPEG_VIDEO_H264_LEVEL_4_0);
    SetIFrameInterval(KEY_FRAME_INTERVAL);

    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_)) {
        ERROR_PRINT("Could not set bitrate");
    }

    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_REPEAT_SEQ_HEADER, true)) {
        ERROR_PRINT("Could not set repeat seq header");
    }

    auto src_memory = is_dma_src ? V4L2_MEMORY_DMABUF : V4L2_MEMORY_MMAP;
    if (!SetupOutputBuffer(width, height, src_pix_fmt, src_memory, BUFFER_NUM)) {
        ERROR_PRINT("Could not setup output buffer");
    }
    if (!SetupCaptureBuffer(width, height, V4L2_PIX_FMT_H264, V4L2_MEMORY_MMAP, BUFFER_NUM)) {
        ERROR_PRINT("Could not setup capture buffer");
    }
}

void V4L2Encoder::ForceKeyFrame() {
    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1)) {
        ERROR_PRINT("Could not force v4l2 encoder to keyframe");
    }
}

void V4L2Encoder::SetLevel(uint32_t level) {
    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_H264_LEVEL, level)) {
        ERROR_PRINT("Could not set h264 level");
    }
}
void V4L2Encoder::SetProfile(uint32_t profile) {
    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_H264_PROFILE, profile)) {
        ERROR_PRINT("Could not set h264 profile to baseline");
    }
}

void V4L2Encoder::SetFps(uint32_t adjusted_fps) {
    if (framerate_ != adjusted_fps) {
        framerate_ = adjusted_fps;
        if (!V4L2Codec::SetFps(framerate_)) {
            ERROR_PRINT("Failed to set output fps: %d", framerate_);
        }
    }
}

void V4L2Encoder::SetRateControlMode(uint32_t mode) {
    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_BITRATE_MODE, mode)) {
        ERROR_PRINT("Could not set rate control mode");
    }
}
void V4L2Encoder::SetIFrameInterval(uint32_t interval) {
    if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_H264_I_PERIOD, interval)) {
        ERROR_PRINT("Could not set i-frame interval");
    }
}

void V4L2Encoder::SetBitrate(uint32_t adjusted_bitrate_bps) {
    if (adjusted_bitrate_bps < 1000000) {
        adjusted_bitrate_bps = 1000000;
    } else {
        adjusted_bitrate_bps = (adjusted_bitrate_bps / 25000) * 25000;
    }

    if (bitrate_bps_ != adjusted_bitrate_bps) {
        bitrate_bps_ = adjusted_bitrate_bps;
        if (!SetExtCtrl(V4L2_CID_MPEG_VIDEO_BITRATE, bitrate_bps_)) {
            ERROR_PRINT("Failed to set bitrate: %d bps", bitrate_bps_);
        }
    }
}
