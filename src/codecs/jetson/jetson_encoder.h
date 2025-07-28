#ifndef JETSON_CODEC_
#define JETSON_CODEC_

#include "common/thread_safe_queue.h"
#include "common/v4l2_frame_buffer.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"

#include <NvVideoEncoder.h>

class JetsonEncoder {
  public:
    JetsonEncoder(int width, int height, uint32_t dst_pix_fmt, bool is_dma_src);
    ~JetsonEncoder();

    static std::unique_ptr<JetsonEncoder> Create(int width, int height, uint32_t dst_pix_fmt,
                                                 bool is_dma_src);
    void EmplaceBuffer(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer,
                       std::function<void(V4L2Buffer &)> on_capture);
    void ForceKeyFrame();
    void SetFps(int adjusted_fps);
    void SetBitrate(int adjusted_bitrate_bps);

  private:
    NvVideoEncoder *encoder_;
    std::atomic<bool> abort_;
    int width_;
    int height_;
    int framerate_;
    int bitrate_bps_;
    int output_plane_fd_[32];
    uint32_t src_pix_fmt_;
    uint32_t dst_pix_fmt_;
    bool is_dma_src_;
    ThreadSafeQueue<std::function<void(V4L2Buffer &)>> capturing_tasks_;

    bool CreateVideoEncoder();
    bool PrepareOutputDmaBuffer(int width, int height);
    bool PrepareCaptureBuffer();
    void Start();
    void SendEOS();
    static bool EncoderCapturePlaneDqCallback(struct v4l2_buffer *v4l2_buf, NvBuffer *buffer,
                                              NvBuffer *shared_buffer, void *arg);
    void ConvertI420ToYUV420M(NvBuffer *nv_buffer,
                              rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer);
};

#endif
