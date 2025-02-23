#include "customized_video_encoder_factory.h"

#include <modules/video_coding/codecs/av1/av1_svc_config.h>
#include <modules/video_coding/codecs/av1/libaom_av1_encoder.h>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/codecs/vp9/include/vp9.h>

#include "v4l2_codecs/v4l2_h264_encoder.h"

std::unique_ptr<webrtc::VideoEncoderFactory> CreateCustomizedVideoEncoderFactory(Args args) {
    return std::make_unique<CustomizedVideoEncoderFactory>(args);
}

std::vector<webrtc::SdpVideoFormat> CustomizedVideoEncoderFactory::GetSupportedFormats() const {
    std::vector<webrtc::SdpVideoFormat> supported_codecs;

    if (args_.hw_accel) {
        // hw h264
        supported_codecs.push_back(CreateH264Format(
            webrtc::H264Profile::kProfileConstrainedBaseline, webrtc::H264Level::kLevel4, "1"));
        supported_codecs.push_back(CreateH264Format(
            webrtc::H264Profile::kProfileConstrainedBaseline, webrtc::H264Level::kLevel4, "0"));
        supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                                                    webrtc::H264Level::kLevel4, "1"));
        supported_codecs.push_back(CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                                                    webrtc::H264Level::kLevel4, "0"));
    } else {
        // vp8
        supported_codecs.push_back(webrtc::SdpVideoFormat(cricket::kVp8CodecName));
        // vp9
        auto supported_vp9_formats = webrtc::SupportedVP9Codecs(true);
        supported_codecs.insert(supported_codecs.end(), std::begin(supported_vp9_formats),
                                std::end(supported_vp9_formats));
        // av1
        supported_codecs.push_back(
            webrtc::SdpVideoFormat(cricket::kAv1CodecName, webrtc::SdpVideoFormat::Parameters(),
                                   webrtc::LibaomAv1EncoderSupportedScalabilityModes()));
        // sw h264
        auto supported_h264_formats = webrtc::SupportedH264Codecs(true);
        supported_codecs.insert(supported_codecs.end(), std::begin(supported_h264_formats),
                                std::end(supported_h264_formats));
    }

    return supported_codecs;
}

std::unique_ptr<webrtc::VideoEncoder>
CustomizedVideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat &format) {
    if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) {
        if (args_.hw_accel) {
            return V4l2H264Encoder::Create(args_);
        } else {
            return webrtc::H264Encoder::Create(cricket::VideoCodec(format));
        }
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp8CodecName)) {
        return webrtc::VP8Encoder::Create();
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kVp9CodecName)) {
        return webrtc::VP9Encoder::Create(cricket::VideoCodec(format));
    } else if (absl::EqualsIgnoreCase(format.name, cricket::kAv1CodecName)) {
        return webrtc::CreateLibaomAv1Encoder();
    }

    return nullptr;
}
