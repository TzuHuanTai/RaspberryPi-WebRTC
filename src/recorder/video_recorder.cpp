#include "recorder/video_recorder.h"

#include <memory>

#include "common/utils.h"
#include "recorder/h264_recorder.h"
#include "recorder/raw_h264_recorder.h"

VideoRecorder::VideoRecorder(Args config, std::string encoder_name)
    : Recorder(),
      encoder_name(encoder_name),
      config(config),
      abort(true) {}

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

void VideoRecorder::OnBuffer(V4l2Buffer &buffer) {
    if (frame_buffer_queue.size() < 8) {
        rtc::scoped_refptr<V4l2FrameBuffer> frame_buffer(
            V4l2FrameBuffer::Create(config.width, config.height, buffer, config.format));
        frame_buffer->CopyBufferData();
        frame_buffer_queue.push(frame_buffer);
    }
}

void VideoRecorder::PostStop() { abort = true; }

void VideoRecorder::SetBaseTimestamp(struct timeval time) { base_time_ = time; }

void VideoRecorder::OnEncoded(V4l2Buffer &buffer) {
    AVPacket *pkt = av_packet_alloc();
    pkt->data = static_cast<uint8_t *>(buffer.start);
    pkt->size = buffer.length;
    pkt->stream_index = st->index;

    double elapsed_time = (buffer.timestamp.tv_sec - base_time_.tv_sec) +
                          (buffer.timestamp.tv_usec - base_time_.tv_usec) / 1000000.0;
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

    if (abort.load() && (frame_buffer->flags() & V4L2_BUF_FLAG_KEYFRAME)) {
        abort.store(false);
        SetBaseTimestamp(frame_buffer->timestamp());
    }

    if (!abort.load()) {
        Encode(frame_buffer);
    }

    return true;
}
