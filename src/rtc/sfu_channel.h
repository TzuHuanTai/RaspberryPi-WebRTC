#ifndef SFU_CHANNEL_H_
#define SFU_CHANNEL_H_

#include <fstream>
#include <map>
#include <vector>

#include <api/data_channel_interface.h>
#include <nlohmann/json.hpp>

#include "common/interface/subject.h"
#include "common/utils.h"
#include "ipc/unix_socket_server.h"
#include "rtc/rtc_channel.h"

class SfuChannel : public RtcChannel {
  public:
    static std::shared_ptr<SfuChannel>
    Create(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

    SfuChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
    ~SfuChannel();

    void OnMessage(const webrtc::DataBuffer &buffer) override;

  protected:
    void Send(const uint8_t *data, size_t size) override;

  private:
    std::string topic_;

    void SendUserData(const std::string &topic, const uint8_t *data, size_t size);
};

#endif // SFU_CHANNEL_H_
