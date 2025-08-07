#ifndef IFRAME_PROCESSOR_H_
#define IFRAME_PROCESSOR_H_

#include <functional>

#include "common/v4l2_frame_buffer.h"

class IFrameProcessor {
  public:
    virtual ~IFrameProcessor() = default;

    /**
     * Submits a frame buffer to a processing unit (e.g., ISP, encoder).
     *
     * This method enqueues the given frame buffer into the hardware pipeline. The callback will be
     * triggered with the resulting buffer when processing is completed.
     *
     * @param frame_buffer Frame buffer to be processed by the device.
     * @param on_capture Callback invoked with the resulting frame buffer.
     */
    virtual void EmplaceBuffer(V4L2FrameBufferRef frame_buffer,
                               std::function<void(V4L2FrameBufferRef)> on_capture) = 0;
};

#endif
