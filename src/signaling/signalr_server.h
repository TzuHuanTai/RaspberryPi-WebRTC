#ifndef SIGNALR_SERVICE_H_
#define SIGNALR_SERVICE_H_

#include "conductor.h"
#include "signaling/signaling_service.h"

#include <signalrclient/hub_connection.h>
#include <signalrclient/hub_connection_builder.h>
#include <signalrclient/signalr_value.h>

struct SignalrTopic {
    std::string offer_sdp = "OfferSDP";
    std::string offer_ice = "OfferICE";
    std::string answer_sdp = "AnswerSDP";
    std::string answer_ice = "AnswerICE";
    std::string connected_client = "ConnectedClient";
    std::string join_as_client = "JoinAsClient";
    std::string join_as_server = "JoinAsServer";
};

class SignalrService : public SignalingService {
public:
    SignalrTopic topics;
    std::mutex mtx;
    std::condition_variable cond_var;
    bool ready = false;

    SignalrService(std::string url, std::shared_ptr<Conductor> conductor);
    ~SignalrService() override {
        std::cout << "=> ~SignalrService: destroied" << std::endl;
    };

    void Connect() override;
    void Disconnect() override;
    SignalrService &AutoReconnect();
    SignalrService &DisconnectOnCompleted();
    void Subscribe(std::string event_name, const signalr::hub_connection::method_invoked_handler &handler);
    void JoinAsClient();
    void JoinAsServer();
    void Start();

protected:
    void AnswerLocalSdp(std::string sdp) override;
    void AnswerLocalIce(std::string sdp_mid,
                                int sdp_mline_index,
                                std::string candidate) override;

private:
    std::string url;
    std::string client_id_;
    signalr::hub_connection connection_;
    std::function<void()> connected_func_;
    void ListenClientId();
    void ListenOfferSdp();
    void ListenOfferIce();
    void SendMessage(std::string method, std::vector<signalr::value> args);
};

#endif
