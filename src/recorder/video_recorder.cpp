#include "recorder/video_recorder.h"

#include <memory>

#include "common/logging.h"
#include "common/utils.h"

VideoRecorder::VideoRecorder(int width, int height, int fps, std::string encoder_name)
    : Recorder(),
      fps(fps),
      width(width),
      height(height),
      encoder_name(encoder_name),
      abort_(true) {}

void VideoRecorder::InitializeEncoderCtx(AVCodecContext *&encoder) {
    frame_rate = {.num = (int)fps, .den = 1};

    const AVCodec *codec = avcodec_find_encoder_by_name(encoder_name.c_str());
    encoder = avcodec_alloc_context3(codec);
    encoder->codec_type = AVMEDIA_TYPE_VIDEO;
    encoder->width = width;
    encoder->height = height;
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

    int64_t elapsed_usec = (int64_t)(timestamp.tv_sec - base_time_.tv_sec) * 1000000LL +
                           (int64_t)(timestamp.tv_usec - base_time_.tv_usec);

    AVRational usec_base = {1, 1000000};
    int64_t calculated_ts = av_rescale_q(elapsed_usec, usec_base, st->time_base);

    static int64_t last_dts = -1;
    if (calculated_ts <= last_dts) {
        calculated_ts = last_dts + 1;
    }
    last_dts = calculated_ts;

    pkt->pts = pkt->dts = calculated_ts;

    OnPacketed(pkt);

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
