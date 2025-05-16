#ifndef V4L2_CODEC_
#define V4L2_CODEC_

#include "common/thread_safe_queue.h"
#include "common/v4l2_utils.h"
#include "common/worker.h"

class V4L2Codec {
  public:
    V4L2Codec();
    ~V4L2Codec();
    void EmplaceBuffer(V4L2Buffer &buffer, std::function<void(V4L2Buffer &)> on_capture);

  protected:
    int fd_;
    V4L2BufferGroup output_;
    V4L2BufferGroup capture_;
    virtual void HandleEvent(){};

    bool Open(const char *file_name);
    bool PrepareBuffer(V4L2BufferGroup *gbuffer, int width, int height, uint32_t pix_fmt,
                       v4l2_buf_type type, v4l2_memory memory, int buffer_num,
                       bool has_dmafd = false);
    void Start();

  private:
    uint32_t dst_fmt_;
    std::atomic<bool> abort_;
    std::unique_ptr<Worker> worker_;
    ThreadSafeQueue<int> output_buffer_index_;
    ThreadSafeQueue<std::function<void(V4L2Buffer &)>> capturing_tasks_;
    const char *file_name_;
    bool CaptureBuffer();
};

#endif // V4L2_CODEC_
