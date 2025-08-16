#include "codecs/jetson/jetson_video_encoder.h"
#include "common/logging.h"
#include "common/v4l2_frame_buffer.h"

#include <modules/video_coding/include/video_codec_interface.h>

std::unique_ptr<webrtc::VideoEncoder> JetsonVideoEncoder::Create(Args args) {
    return std::make_unique<JetsonVideoEncoder>(args);
}

JetsonVideoEncoder::JetsonVideoEncoder(Args args)
    : fps_adjuster_(args.fps),
      bitrate_adjuster_(.85, 1),
      callback_(nullptr) {}

int32_t JetsonVideoEncoder::InitEncode(const webrtc::VideoCodec *codec_settings,
                                       const VideoEncoder::Settings &settings) {
    codec_ = *codec_settings;
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    bitrate_adjuster_.SetTargetBitrateBps(codec_settings->startBitrate * 1000);

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t JetsonVideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback *callback) {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t JetsonVideoEncoder::Release() {
    encoder_.reset();
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t JetsonVideoEncoder::Encode(const webrtc::VideoFrame &frame,
                                   const std::vector<webrtc::VideoFrameType> *frame_types) {
    if (!frame_types) {
        return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
    }

    if ((*frame_types)[0] == webrtc::VideoFrameType::kEmptyFrame) {
        return WEBRTC_VIDEO_CODEC_OK;
    }
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer = frame.video_frame_buffer();
    auto v4l2_frame_buffer = V4L2FrameBufferRef(static_cast<V4L2FrameBuffer *>(frame_buffer.get()));

    if (!encoder_) {
        auto codec_fmt = GetV4L2CodecFormat(codec_.codecType);
        if (codec_fmt == 0) {
            return WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
        }
        encoder_ =
            JetsonEncoder::Create(width_, height_, codec_fmt,
                                  frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kNative);
    }

    if ((*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey) {
        encoder_->ForceKeyFrame();
    }

    encoder_->EmplaceBuffer(v4l2_frame_buffer, [this, frame](V4L2FrameBufferRef encoded_buffer) {
        auto v4l2buffer = encoded_buffer->GetRawBuffer();
        SendFrame(frame, v4l2buffer);
    });

    return WEBRTC_VIDEO_CODEC_OK;
}

void JetsonVideoEncoder::SetRates(const RateControlParameters &parameters) {
    if (parameters.bitrate.get_sum_bps() <= 0 || parameters.framerate_fps <= 0) {
        return;
    }
    bitrate_adjuster_.SetTargetBitrateBps(parameters.bitrate.get_sum_bps());
    fps_adjuster_ = parameters.framerate_fps;

    if (!encoder_) {
        return;
    }
    encoder_->SetFps(fps_adjuster_);
    encoder_->SetBitrate(bitrate_adjuster_.GetAdjustedBitrateBps());
}

webrtc::VideoEncoder::EncoderInfo JetsonVideoEncoder::GetEncoderInfo() const {
    EncoderInfo info;
    info.supports_native_handle = true;
    info.is_hardware_accelerated = true;
    info.implementation_name = "Jetson Hardware Encoder";
    return info;
}

void JetsonVideoEncoder::SendFrame(const webrtc::VideoFrame &frame, V4L2Buffer &encoded_buffer) {
    auto encoded_image_buffer =
        webrtc::EncodedImageBuffer::Create((uint8_t *)encoded_buffer.start, encoded_buffer.length);

    webrtc::CodecSpecificInfo codec_specific;
    codec_specific.codecType = codec_.codecType;

    if (codec_specific.codecType == webrtc::kVideoCodecH264) {
        codec_specific.codecSpecific.H264.packetization_mode =
            webrtc::H264PacketizationMode::NonInterleaved;
    }

    encoded_image_.SetEncodedData(encoded_image_buffer);
    encoded_image_.SetTimestamp(frame.timestamp());
    encoded_image_.SetColorSpace(frame.color_space());
    encoded_image_._encodedWidth = width_;
    encoded_image_._encodedHeight = height_;
    encoded_image_.capture_time_ms_ = frame.render_time_ms();
    encoded_image_.ntp_time_ms_ = frame.ntp_time_ms();
    encoded_image_.rotation_ = frame.rotation();
    encoded_image_._frameType = encoded_buffer.flags & V4L2_BUF_FLAG_KEYFRAME
                                    ? webrtc::VideoFrameType::kVideoFrameKey
                                    : webrtc::VideoFrameType::kVideoFrameDelta;

    auto result = callback_->OnEncodedImage(encoded_image_, &codec_specific);
    if (result.error != webrtc::EncodedImageCallback::Result::OK) {
        ERROR_PRINT("Failed to send the frame => %d", result.error);
    }
}

uint32_t JetsonVideoEncoder::GetV4L2CodecFormat(webrtc::VideoCodecType type) {
    switch (type) {
        case webrtc::kVideoCodecVP8:
            return V4L2_PIX_FMT_VP8;
        case webrtc::kVideoCodecVP9:
            return V4L2_PIX_FMT_VP9;
        case webrtc::kVideoCodecAV1:
            return V4L2_PIX_FMT_AV1;
        case webrtc::kVideoCodecH264:
            return V4L2_PIX_FMT_H264;
        default:
            return 0;
    }
}
