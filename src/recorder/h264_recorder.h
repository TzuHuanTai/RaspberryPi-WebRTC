#ifndef H264_RECODER_H_
#define H264_RECODER_H_

#include "recorder/video_recorder.h"
#include "v4l2_codecs/v4l2_decoder.h"
#include "v4l2_codecs/v4l2_encoder.h"

class H264Recorder : public VideoRecorder {
public:
    static std::unique_ptr<H264Recorder> Create(std::shared_ptr<V4L2Capture> capture);
    H264Recorder(std::shared_ptr<V4L2Capture> capture, std::string encoder_name);
    ~H264Recorder();

protected:
    void Encode(Buffer buffer) override;

private:
    int frame_count_;
    std::unique_ptr<V4l2Decoder> decoder_;
    std::unique_ptr<V4l2Encoder> encoder_;

    void ResetCodecs();
};

#endif
