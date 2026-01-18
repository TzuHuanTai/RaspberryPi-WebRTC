#include "recorder/raw_h264_recorder.h"
#include "common/logging.h"

#define NAL_UNIT_TYPE_IDR 5
#define NAL_UNIT_TYPE_SPS 7
#define NAL_UNIT_TYPE_PPS 8

std::unique_ptr<RawH264Recorder> RawH264Recorder::Create(int width, int height, int fps) {
    return std::make_unique<RawH264Recorder>(width, height, fps, "h264_v4l2m2m");
}

RawH264Recorder::RawH264Recorder(int width, int height, int fps, std::string encoder_name)
    : VideoRecorder(width, height, fps, encoder_name),
      has_sps_(false),
      has_pps_(false),
      has_first_keyframe_(false){};

RawH264Recorder::~RawH264Recorder() {}

void RawH264Recorder::OnStart() {
    has_sps_ = false;
    has_pps_ = false;
    has_first_keyframe_ = false;
}

void RawH264Recorder::ReleaseEncoder() {}

void RawH264Recorder::Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (frame_buffer->flags() & V4L2_BUF_FLAG_KEYFRAME && !has_first_keyframe_) {
        CheckNALUnits(frame_buffer->Data(), frame_buffer->size());
        if (has_sps_ && has_pps_) {
            has_first_keyframe_ = true;
        }
    }

    if (has_first_keyframe_) {
        OnEncoded((uint8_t *)frame_buffer->Data(), frame_buffer->size(), frame_buffer->timestamp());
    }
}

bool RawH264Recorder::CheckNALUnits(const void *start, uint32_t length) {
    if (start == nullptr || length < 4) {
        return false;
    }

    const uint8_t *data = static_cast<const uint8_t *>(start);
    for (uint32_t i = 0; i < length - 4; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            ((data[i + 2] == 0x01) || (data[i + 2] == 0x00 && data[i + 3] == 0x01))) {
            // Found start code
            size_t startCodeSize = (data[i + 2] == 0x01) ? 3 : 4;
            uint8_t nalUnitType = data[i + startCodeSize] & 0x1F;
            switch (nalUnitType) {
                case NAL_UNIT_TYPE_IDR:
                    DEBUG_PRINT("Found IDR frame");
                    break;
                case NAL_UNIT_TYPE_SPS:
                    DEBUG_PRINT("Found SPS NAL unit");
                    has_sps_ = true;
                    break;
                case NAL_UNIT_TYPE_PPS:
                    DEBUG_PRINT("Found PPS NAL unit");
                    has_pps_ = true;
                    break;
                default:
                    break;
            }
            i += startCodeSize;
        }
    }

    return false;
}
