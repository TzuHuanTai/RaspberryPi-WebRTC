#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

// Interface implemented by every signaling backend. Services own its own peers, and reconnection.
class SignalingService {
  public:
    virtual ~SignalingService() = default;

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;
};

#endif
