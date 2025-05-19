#include "codecs/v4l2/v4l2_h264_encoder.h"
#include "common/logging.h"
#include "common/v4l2_frame_buffer.h"

std::unique_ptr<webrtc::VideoEncoder> V4L2H264Encoder::Create(Args args) {
    return std::make_unique<V4L2H264Encoder>(args);
}

V4L2H264Encoder::V4L2H264Encoder(Args args)
    : fps_adjuster_(args.fps),
      bitrate_adjuster_(.85, 1),
      callback_(nullptr) {}

int32_t V4L2H264Encoder::InitEncode(const webrtc::VideoCodec *codec_settings,
                                    const VideoEncoder::Settings &settings) {
    codec_ = *codec_settings;
    width_ = codec_settings->width;
    height_ = codec_settings->height;
    bitrate_adjuster_.SetTargetBitrateBps(codec_settings->startBitrate * 1000);

    encoded_image_.timing_.flags = webrtc::VideoSendTiming::TimingFrameFlags::kInvalid;
    encoded_image_.content_type_ = webrtc::VideoContentType::UNSPECIFIED;

    if (codec_.codecType != webrtc::kVideoCodecH264) {
        return WEBRTC_VIDEO_CODEC_ERROR;
    }

    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4L2H264Encoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback *callback) {
    callback_ = callback;
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4L2H264Encoder::Release() {
    encoder_.reset();
    return WEBRTC_VIDEO_CODEC_OK;
}

int32_t V4L2H264Encoder::Encode(const webrtc::VideoFrame &frame,
                                const std::vector<webrtc::VideoFrameType> *frame_types) {
    if (!frame_types) {
        return WEBRTC_VIDEO_CODEC_NO_OUTPUT;
    }

    if ((*frame_types)[0] == webrtc::VideoFrameType::kEmptyFrame) {
        return WEBRTC_VIDEO_CODEC_OK;
    }
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer = frame.video_frame_buffer();

    V4L2Buffer src_buffer;
    if (frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kNative) {
        V4L2FrameBuffer *raw_buffer = static_cast<V4L2FrameBuffer *>(frame_buffer.get());
        src_buffer = raw_buffer->GetRawBuffer();
    } else {
        auto i420_buffer = frame_buffer->GetI420();
        unsigned int i420_buffer_size =
            (i420_buffer->StrideY() * height_) +
            ((i420_buffer->StrideY() + 1) / 2) * ((height_ + 1) / 2) * 2;

        src_buffer.start = const_cast<uint8_t *>(i420_buffer->DataY());
        src_buffer.length = i420_buffer_size;
    }

    if (!encoder_) {
        encoder_ = V4L2Encoder::Create(width_, height_, V4L2_PIX_FMT_YUV420, src_buffer.dmafd > 0);
    }

    if ((*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey) {
        V4L2Util::SetExtCtrl(encoder_->GetFd(), V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1);
    }

    encoder_->EmplaceBuffer(src_buffer, [this, frame](V4L2Buffer &encoded_buffer) {
        SendFrame(frame, encoded_buffer);
    });

    return WEBRTC_VIDEO_CODEC_OK;
}

void V4L2H264Encoder::SetRates(const RateControlParameters &parameters) {
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

webrtc::VideoEncoder::EncoderInfo V4L2H264Encoder::GetEncoderInfo() const {
    EncoderInfo info;
    info.supports_native_handle = true;
    info.is_hardware_accelerated = true;
    info.implementation_name = "Raspberry Pi V4L2 H264 Hardware Encoder";
    return info;
}

void V4L2H264Encoder::SendFrame(const webrtc::VideoFrame &frame, V4L2Buffer &encoded_buffer) {
    auto encoded_image_buffer =
        webrtc::EncodedImageBuffer::Create((uint8_t *)encoded_buffer.start, encoded_buffer.length);

    webrtc::CodecSpecificInfo codec_specific;
    codec_specific.codecType = webrtc::kVideoCodecH264;
    codec_specific.codecSpecific.H264.packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;

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
