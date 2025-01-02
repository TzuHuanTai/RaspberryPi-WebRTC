#include "recorder/raw_h264_recorder.h"
#include "common/logging.h"

#define NAL_UNIT_TYPE_IDR 5
#define NAL_UNIT_TYPE_SPS 7
#define NAL_UNIT_TYPE_PPS 8

const int SECOND_PER_FILE = 300;

std::unique_ptr<RawH264Recorder> RawH264Recorder::Create(Args config) {
    auto ptr = std::make_unique<RawH264Recorder>(config, "h264_v4l2m2m");
    ptr->Initialize();
    return ptr;
}

RawH264Recorder::RawH264Recorder(Args config, std::string encoder_name)
    : VideoRecorder(config, encoder_name),
      has_sps_(false),
      has_pps_(false),
      has_first_keyframe_(false){};

RawH264Recorder::~RawH264Recorder() {}

void RawH264Recorder::PreStart() {
    has_sps_ = false;
    has_pps_ = false;
    has_first_keyframe_ = false;
}

void RawH264Recorder::PostStop() {
    // Wait P-frames are all consumed until I-frame appear.
    auto frame = frame_buffer_queue.front();
    while (frame && (frame.value()->flags() & V4L2_BUF_FLAG_KEYFRAME) != 0) {
        ConsumeBuffer();
    }
    has_first_keyframe = false;
}

void RawH264Recorder::Encode(rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer) {
    V4l2Buffer buffer((void *)frame_buffer->Data(), frame_buffer->size(), frame_buffer->flags(),
                      frame_buffer->timestamp());

    if (buffer.flags & V4L2_BUF_FLAG_KEYFRAME && !has_first_keyframe_) {
        CheckNALUnits(buffer);
        if (has_sps_ && has_pps_) {
            has_first_keyframe_ = true;
        }
    }

    if (has_first_keyframe_) {
        OnEncoded(buffer);
    }
}

bool RawH264Recorder::CheckNALUnits(const V4l2Buffer &buffer) {
    if (buffer.start == nullptr || buffer.length < 4) {
        return false;
    }

    uint8_t *data = static_cast<uint8_t *>(buffer.start);
    for (unsigned int i = 0; i < buffer.length - 4; ++i) {
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
