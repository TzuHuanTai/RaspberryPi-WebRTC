#include "recorder/video_recorder.h"

#include <memory>

#include "common/logging.h"
#include "common/utils.h"

VideoRecorder::VideoRecorder(Args config, std::string encoder_name)
    : Recorder(),
      encoder_name(encoder_name),
      config(config),
      abort_(true) {}

void VideoRecorder::InitializeEncoderCtx(AVCodecContext *&encoder) {
    frame_rate = {.num = (int)config.fps, .den = 1};

    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_VIDEO;
    encoder->width = config.width;
    encoder->height = config.height;
    encoder->framerate = frame_rate;
    encoder->time_base = av_inv_q(frame_rate);
    encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

void VideoRecorder::OnBuffer(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) {
    if (!frame_buffer_queue.push(frame_buffer->Clone())) {
        INFO_PRINT("frame_buffer_queue skip a frame due to overloaded queue.\n");
    }
}

void VideoRecorder::OnStop() {
    // Wait P-frames are all consumed until I-frame appear when the video source is h264.
    auto frame = frame_buffer_queue.front();
    while (frame.has_value() && (frame.value()->format() == V4L2_PIX_FMT_H264 &&
                                 (frame.value()->flags() & V4L2_BUF_FLAG_KEYFRAME) != 0)) {
        ConsumeBuffer();
    }

    abort_ = true;

    {
        std::lock_guard<std::mutex> lock(encoder_mtx_);
        ReleaseEncoder();
    }
}

void VideoRecorder::SetBaseTimestamp(struct timeval time) { base_time_ = time; }

void VideoRecorder::OnEncoded(uint8_t *start, uint32_t length, timeval timestamp) {
    AVPacket *pkt = av_packet_alloc();
    pkt->data = start;
    pkt->size = length;
    pkt->stream_index = st->index;

    double elapsed_time = (timestamp.tv_sec - base_time_.tv_sec) +
                          (timestamp.tv_usec - base_time_.tv_usec) / 1000000.0;
    pkt->pts = pkt->dts =
        static_cast<int64_t>(elapsed_time * st->time_base.den / st->time_base.num);

    OnPacketed(pkt);
    av_packet_unref(pkt);
    av_packet_free(&pkt);
}

bool VideoRecorder::ConsumeBuffer() {
    auto item = frame_buffer_queue.pop();

    if (!item) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return false;
    }

    auto frame_buffer = item.value();

    if (abort_) {
        abort_ = false;
        SetBaseTimestamp(frame_buffer->timestamp());
    }

    if (!abort_) {
        std::lock_guard<std::mutex> lock(encoder_mtx_);
        Encode(frame_buffer);
    }

    return true;
}
