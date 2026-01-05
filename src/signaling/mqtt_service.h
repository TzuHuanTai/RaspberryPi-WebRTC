#ifndef MQTT_SERVICE_H_
#define MQTT_SERVICE_H_

#include "signaling/signaling_service.h"

#include <memory>
#include <mosquitto.h>

#include "args.h"

class MqttService : public SignalingService {
  public:
    static std::shared_ptr<MqttService> Create(Args args, std::shared_ptr<Conductor> conductor);

    MqttService(Args args, std::shared_ptr<Conductor> conductor);
    ~MqttService();

  protected:
    void Connect() override;
    void Disconnect() override;
    void RefreshPeerMap() override;

  private:
    int port_;
    std::string uid_;
    std::string hostname_;
    std::string username_;
    std::string password_;
    std::string sdp_base_topic_;
    std::string ice_base_topic_;
    struct mosquitto *connection_;
    std::string status_topic_;

    std::unordered_map<std::string, std::string> client_id_to_peer_id_;
    std::unordered_map<std::string, std::string> peer_id_to_client_id_;

    void OnRemoteSdp(const std::string &peer_id, const std::string &message);
    void OnRemoteIce(const std::string &peer_id, const std::string &message);
    void AnswerLocalSdp(const std::string &peer_id, const std::string &sdp,
                        const std::string &type);
    void AnswerLocalIce(const std::string &peer_id, const std::string &sdp_mid,
                        const int sdp_mline_index, const std::string &candidate);

    void Subscribe(const std::string &topic);
    void Unsubscribe(const std::string &topic);
    void Publish(const std::string &topic, const std::string &msg);
    void PublishRetain(const std::string& topic, const std::string& msg, int qos, bool retain);
    void OnConnect(struct mosquitto *mosq, void *obj, int result);
    void OnMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
    std::string GetClientId(std::string &topic);
    std::string GetTopic(const std::string &topic, const std::string &client_id = "") const;
    void ConfigureLwt();
};

#endif
