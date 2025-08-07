#ifndef V4L2_CODEC_
#define V4L2_CODEC_

#include "common/interface/processor.h"
#include "common/thread_safe_queue.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"

class V4L2Codec : public IFrameProcessor {
  public:
    V4L2Codec();
    ~V4L2Codec() override;

    void EmplaceBuffer(V4L2FrameBufferRef buffer,
                       std::function<void(V4L2FrameBufferRef)> on_capture) override;

  protected:
    bool Open(const char *file_name);
    bool SetFps(uint32_t fps);
    bool SetExtCtrl(uint32_t id, int32_t value);
    bool SetupOutputBuffer(int width, int height, uint32_t pix_fmt, v4l2_memory memory,
                           int buffer_num);
    bool SetupCaptureBuffer(int width, int height, uint32_t pix_fmt, v4l2_memory memory,
                            int buffer_num, bool exp_dmafd = false);
    bool SubscribeEvent(uint32_t ev_type);
    virtual void HandleEvent();
    void Start();

  private:
    int fd_;
    int width_;
    int height_;
    uint32_t dst_fmt_;
    const char *file_name_;
    V4L2BufferGroup output_;
    V4L2BufferGroup capture_;
    std::atomic<bool> abort_;
    std::unique_ptr<Worker> worker_;
    ThreadSafeQueue<int> output_buffer_index_;
    ThreadSafeQueue<std::function<void(V4L2FrameBufferRef)>> capturing_tasks_;

    bool PrepareBuffer(V4L2BufferGroup *gbuffer, int width, int height, uint32_t pix_fmt,
                       v4l2_buf_type type, v4l2_memory memory, int buffer_num,
                       bool has_dmafd = false);
    bool CaptureBuffer();
};

#endif // V4L2_CODEC_
