#include "codecs/h264/openh264_encoder.h"

#include "common/logging.h"
#include "common/utils.h"

std::unique_ptr<Openh264Encoder> Openh264Encoder::Create(EncoderConfig config) {
    auto ptr = std::make_unique<Openh264Encoder>(config);
    if (!ptr->Init()) {
        return nullptr;
    }
    return ptr;
}

Openh264Encoder::Openh264Encoder(EncoderConfig config)
    : config_(config),
      encoder_(nullptr) {}

Openh264Encoder::~Openh264Encoder() {
    encoder_->Uninitialize();
    WelsDestroySVCEncoder(encoder_);
    DEBUG_PRINT("sw h264 encode was released!\n");
}

bool Openh264Encoder::Init() {
    int rv = WelsCreateSVCEncoder(&encoder_);
    if (rv != 0) {
        ERROR_PRINT("Failed to create OpenH264 encoder.");
        return false;
    }

    SEncParamExt encoder_param;
    encoder_->GetDefaultParams(&encoder_param);
    encoder_param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encoder_param.iTemporalLayerNum = 1;
    encoder_param.uiIntraPeriod = config_.keyframe_interval;
    encoder_param.uiMaxNalSize = 0;
    encoder_param.iRCMode =
        (config_.rc_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) ? RC_QUALITY_MODE : RC_BITRATE_MODE;
    encoder_param.bEnableFrameSkip = true;
    encoder_param.iMinQp = 18;
    encoder_param.iMaxQp = 40;
    encoder_param.fMaxFrameRate = config_.fps;
    encoder_param.iTargetBitrate = config_.bitrate;
    encoder_param.iMaxBitrate = config_.bitrate * 1.2;

    encoder_param.iMultipleThreadIdc = 4;
    encoder_param.iComplexityMode = LOW_COMPLEXITY;
    encoder_param.iEntropyCodingModeFlag = 0;

    encoder_param.iSpatialLayerNum = 1;
    SSpatialLayerConfig *spartialLayerConfiguration = &encoder_param.sSpatialLayers[0];
    spartialLayerConfiguration->sSliceArgument.uiSliceMode = SM_FIXEDSLCNUM_SLICE;
    spartialLayerConfiguration->sSliceArgument.uiSliceNum = 4;
    spartialLayerConfiguration->uiProfileIdc = PRO_BASELINE;
    encoder_param.iPicWidth = spartialLayerConfiguration->iVideoWidth = config_.width;
    encoder_param.iPicHeight = spartialLayerConfiguration->iVideoHeight = config_.height;
    encoder_param.fMaxFrameRate = spartialLayerConfiguration->fFrameRate = config_.fps;
    encoder_param.iTargetBitrate = spartialLayerConfiguration->iSpatialBitrate = config_.bitrate;
    encoder_param.iMaxBitrate = spartialLayerConfiguration->iMaxSpatialBitrate =
        config_.bitrate * 1.2;

    rv = encoder_->InitializeExt(&encoder_param);
    if (rv != 0) {
        ERROR_PRINT("Failed to initialize OpenH264 encoder.");
        return false;
    }
    return true;
}

void Openh264Encoder::Encode(webrtc::scoped_refptr<webrtc::I420BufferInterface> frame_buffer,
                             std::function<void(uint8_t *, int, bool)> on_capture) {
    src_pic_ = {0};
    src_pic_.iPicWidth = config_.width;
    src_pic_.iPicHeight = config_.height;
    src_pic_.iColorFormat = videoFormatI420;
    src_pic_.iStride[0] = frame_buffer->StrideY();
    src_pic_.iStride[1] = frame_buffer->StrideU();
    src_pic_.iStride[2] = frame_buffer->StrideV();
    src_pic_.pData[0] = const_cast<uint8_t *>(frame_buffer->DataY());
    src_pic_.pData[1] = const_cast<uint8_t *>(frame_buffer->DataU());
    src_pic_.pData[2] = const_cast<uint8_t *>(frame_buffer->DataV());

    SFrameBSInfo info;
    memset(&info, 0, sizeof(SFrameBSInfo));
    int rv = encoder_->EncodeFrame(&src_pic_, &info);

    if (info.eFrameType != videoFrameTypeSkip) {
        int required_capacity = 0;
        for (int i = 0; i < info.iLayerNum; i++) {
            const SLayerBSInfo *layer = &info.sLayerInfo[i];
            for (int nal = 0; nal < layer->iNalCount; ++nal) {
                required_capacity += layer->pNalLengthInByte[nal];
            }
        }

        if (encoded_buf_.capacity() < required_capacity) {
            encoded_buf_.reserve(required_capacity);
        }
        encoded_buf_.resize(required_capacity);

        int encoded_size = 0;
        for (int i = 0; i < info.iLayerNum; i++) {
            const SLayerBSInfo *layer = &info.sLayerInfo[i];
            int layer_len = 0;
            for (int nal = 0; nal < layer->iNalCount; ++nal) {
                layer_len += layer->pNalLengthInByte[nal];
            }

            memcpy(encoded_buf_.data() + encoded_size, layer->pBsBuf, layer_len);
            encoded_size += layer_len;
        }

        bool is_keyframe = (info.eFrameType == videoFrameTypeIDR);
        on_capture(encoded_buf_.data(), encoded_size, is_keyframe);
    }
}
