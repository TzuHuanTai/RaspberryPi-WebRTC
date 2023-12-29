#ifndef MQTT_SERVICE_H_
#define MQTT_SERVICE_H_

#include "conductor.h"
#include "signaling/signaling_service.h"

#include <mosquitto.h>

struct MqttTopic {
    std::string offer_sdp = "OfferSDP";
    std::string offer_ice = "OfferICE";
    std::string answer_sdp = "AnswerSDP";
    std::string answer_ice = "AnswerICE";
};

class MqttService : public SignalingService {
public:
    static std::unique_ptr<MqttService> Create(Args args,
                                std::shared_ptr<Conductor> conductor);
    MqttTopic topics;
    std::mutex mtx;
    std::condition_variable cond_var;
    bool ready = false;

    MqttService(Args args, std::shared_ptr<Conductor> conductor);
    ~MqttService() override;

    void Connect() override;
    void Disconnect() override;

protected:
    void AnswerLocalSdp(std::string sdp) override;
    void AnswerLocalIce(std::string sdp_mid,
                                int sdp_mline_index,
                                std::string candidate) override;

private:
    int port_;
    std::string hostname_;
    std::string username_;
    std::string password_;
    struct mosquitto *connection_;
    void ListenOfferSdp(std::string message);
    void ListenOfferIce(std::string message);
    void Subscribe(const std::string& topic);
    void OnConnect(struct mosquitto *mosq, void *obj, int result);
    void OnMessage(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message);
};

#endif
