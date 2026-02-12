#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include <fstream>
#include <map>
#include <vector>

#include "proto/packet.pb.h"
#include <api/data_channel_interface.h>

#include "common/interface/subject.h"
#include "common/utils.h"
#include "ipc/unix_socket_server.h"

class RtcChannel : public webrtc::DataChannelObserver,
                   public std::enable_shared_from_this<RtcChannel> {
  public:
    using CommandHandler =
        std::function<void(std::shared_ptr<RtcChannel>, const protocol::Packet &)>;
    using CustomPayloadHandler = std::function<void(const std::string &)>;

    static std::shared_ptr<RtcChannel>
    Create(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

    RtcChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
    ~RtcChannel();

    std::string id() const;
    std::string label() const;

    // webrtc::DataChannelObserver
    void OnStateChange() override;
    void OnMessage(const webrtc::DataBuffer &buffer) override;
    void OnClosed(std::function<void()> func);

    void Terminate();
    void RegisterHandler(protocol::CommandType type, CommandHandler func);
    void RegisterHandler(CustomPayloadHandler func);

    void Send(const protocol::QueryFileResponse &response);
    void Send(Buffer image);
    void Send(std::ifstream &file);
    void Send(const std::string &message);

  protected:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;

    virtual void Send(const uint8_t *data, size_t size);
    void Next(const std::string &message);

  private:
    std::string id_;
    std::string label_;
    std::function<void()> on_closed_func_;

    Subject<std::string> custom_cmd_subject_;
    std::vector<Subscription> subscriptions_;
    std::map<protocol::CommandType, Subject<protocol::Packet>> observers_map_;

    void SendStream(protocol::CommandType type, const uint8_t *data, size_t size);
};

#endif // DATA_CHANNEL_H_
