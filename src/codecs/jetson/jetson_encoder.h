#ifndef JETSON_CODEC_
#define JETSON_CODEC_

#include "common/interface/processor.h"
#include "common/thread_safe_queue.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"

#include <NvVideoEncoder.h>

struct JetsonEncoderConfig {
    int width;
    int height;
    bool is_dma_src;
    uint32_t dst_pix_fmt;

    int fps = 30;
    int bitrate = 2 * 1024 * 1024;
    int i_interval = 0;
    int idr_interval = 256;
    v4l2_mpeg_video_bitrate_mode rc_mode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
};

class JetsonEncoder : public IFrameProcessor {
  public:
    static std::unique_ptr<JetsonEncoder> Create(int width, int height, uint32_t dst_pix_fmt,
                                                 bool is_dma_src);
    static std::unique_ptr<JetsonEncoder> Create(JetsonEncoderConfig config);

    JetsonEncoder(JetsonEncoderConfig config, const char *name);
    ~JetsonEncoder() override;

    void EmplaceBuffer(V4L2FrameBufferRef frame_buffer,
                       std::function<void(V4L2FrameBufferRef)> on_capture) override;
    void ForceKeyFrame();
    void SetFps(int adjusted_fps);
    void SetBitrate(int adjusted_bitrate_bps);

  private:
    NvVideoEncoder *encoder_;
    std::atomic<bool> abort_;
    const char *name_;
    int width_;
    int height_;
    int framerate_;
    int bitrate_bps_;
    int i_interval_;
    int idr_interval_;
    uint32_t src_pix_fmt_;
    uint32_t dst_pix_fmt_;
    bool is_dma_src_;
    v4l2_mpeg_video_bitrate_mode rate_control_mode_;
    ThreadSafeQueue<std::function<void(V4L2FrameBufferRef)>> capturing_tasks_;

    bool CreateVideoEncoder();
    bool PrepareCaptureBuffer();
    void Start();
    void SendEOS();
    static bool EncoderCapturePlaneDqCallback(struct v4l2_buffer *v4l2_buf, NvBuffer *buffer,
                                              NvBuffer *shared_buffer, void *arg);
    void ConvertI420ToYUV420M(NvBuffer *nv_buffer,
                              rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer);
    uint32_t DisableAV1IVF();
};

#endif
