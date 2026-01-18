#include "codecs/h264/openh264_encoder.h"

#include "common/logging.h"
#include "common/utils.h"

std::unique_ptr<Openh264Encoder> Openh264Encoder::Create(int width, int height, int fps) {
    auto ptr = std::make_unique<Openh264Encoder>(width, height, fps);
    ptr->Init();
    return ptr;
}

Openh264Encoder::Openh264Encoder(int width, int height, int fps)
    : fps_(fps),
      width_(width),
      height_(height),
      bitrate_(width_ * height_ * fps_ * 0.1),
      encoder_(nullptr) {}

Openh264Encoder::~Openh264Encoder() {
    encoder_->Uninitialize();
    WelsDestroySVCEncoder(encoder_);
    DEBUG_PRINT("sw h264 encode was released!\n");
}

void Openh264Encoder::Init() {
    int rv = WelsCreateSVCEncoder(&encoder_);
    if (rv != 0) {
        ERROR_PRINT("Failed to create OpenH264 encoder.");
        return;
    }

    SEncParamExt encoder_param;
    encoder_->GetDefaultParams(&encoder_param);
    encoder_param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    encoder_param.iTemporalLayerNum = 0;
    encoder_param.uiIntraPeriod = 60;
    encoder_param.iRCMode = RC_BITRATE_MODE;
    encoder_param.bEnableFrameSkip = true;
    encoder_param.iMinQp = 18;
    encoder_param.iMaxQp = 40;
    encoder_param.fMaxFrameRate = fps_;
    encoder_param.iTargetBitrate = bitrate_;
    encoder_param.iMaxBitrate = bitrate_ * 1.2;

    encoder_param.iSpatialLayerNum = 1;
    SSpatialLayerConfig *spartialLayerConfiguration = &encoder_param.sSpatialLayers[0];
    spartialLayerConfiguration->uiProfileIdc = PRO_HIGH;
    encoder_param.iPicWidth = spartialLayerConfiguration->iVideoWidth = width_;
    encoder_param.iPicHeight = spartialLayerConfiguration->iVideoHeight = height_;
    encoder_param.fMaxFrameRate = spartialLayerConfiguration->fFrameRate = fps_;
    encoder_param.iTargetBitrate = spartialLayerConfiguration->iSpatialBitrate = bitrate_;
    encoder_param.iMaxBitrate = spartialLayerConfiguration->iMaxSpatialBitrate = bitrate_ * 1.2;

    rv = encoder_->InitializeExt(&encoder_param);
    if (rv != 0) {
        ERROR_PRINT("Failed to initialize OpenH264 encoder.");
        return;
    }
}

void Openh264Encoder::Encode(rtc::scoped_refptr<webrtc::I420BufferInterface> frame_buffer,
                             std::function<void(uint8_t *, int)> on_capture) {
    src_pic_ = {0};
    src_pic_.iPicWidth = width_;
    src_pic_.iPicHeight = height_;
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

        std::vector<uint8_t> encoded_buf(required_capacity);

        int encoded_size = 0;
        for (int i = 0; i < info.iLayerNum; i++) {
            const SLayerBSInfo *layer = &info.sLayerInfo[i];
            int layer_len = 0;
            for (int nal = 0; nal < layer->iNalCount; ++nal) {
                layer_len += layer->pNalLengthInByte[nal];
            }

            memcpy(encoded_buf.data() + encoded_size, layer->pBsBuf, layer_len);
            encoded_size += layer_len;
        }

        on_capture(encoded_buf.data(), encoded_size);
    }
}
