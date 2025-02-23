#ifndef V4L2_H264_ENCODER_H_
#define V4L2_H264_ENCODER_H_

// WebRTC
#include <api/video_codecs/video_encoder.h>
#include <common_video/include/bitrate_adjuster.h>
#include <modules/video_coding/codecs/h264/include/h264.h>

#include "args.h"
#include "v4l2_codecs/v4l2_encoder.h"

class V4l2H264Encoder : public webrtc::VideoEncoder {
  public:
    static std::unique_ptr<webrtc::VideoEncoder> Create(Args args);
    V4l2H264Encoder(Args args);

    int32_t InitEncode(const webrtc::VideoCodec *codec_settings,
                       const VideoEncoder::Settings &settings) override;
    int32_t RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback *callback) override;
    int32_t Release() override;
    int32_t Encode(const webrtc::VideoFrame &frame,
                   const std::vector<webrtc::VideoFrameType> *frame_types) override;
    void SetRates(const RateControlParameters &parameters) override;
    webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

  protected:
    int width_;
    int height_;
    int fps_adjuster_;
    bool is_dma_;
    std::string name_;
    webrtc::VideoCodec codec_;
    webrtc::EncodedImage encoded_image_;
    webrtc::EncodedImageCallback *callback_;
    webrtc::BitrateAdjuster bitrate_adjuster_;
    std::unique_ptr<V4l2Encoder> encoder_;

    virtual void SendFrame(const webrtc::VideoFrame &frame, V4l2Buffer &encoded_buffer);
};

#endif
