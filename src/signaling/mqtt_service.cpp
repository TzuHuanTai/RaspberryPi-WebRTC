#include "signaling/mqtt_service.h"

#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mqtt_protocol.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unistd.h>

#include "common/logging.h"

namespace {

bool FileReadable(const std::string &p) { return !p.empty() && ::access(p.c_str(), R_OK) == 0; }
bool DirReadable(const std::string &p) {
    return !p.empty() && ::access(p.c_str(), R_OK | X_OK) == 0;
}

std::string EnvOrEmpty(const char *key) {
    const char *v = std::getenv(key);
    return v ? std::string(v) : std::string();
}

std::string PickFirstReadableFile(const std::vector<std::string> &candidates) {
    for (const auto &p : candidates) {
        if (FileReadable(p))
            return p;
    }
    return {};
}

std::string PickFirstReadableDir(const std::vector<std::string> &candidates) {
    for (const auto &p : candidates) {
        if (DirReadable(p))
            return p;
    }
    return {};
}

int ConfigureTls(struct mosquitto *mosq) {
    std::string cafile = EnvOrEmpty("MQTT_CAFILE");
    std::string capath = EnvOrEmpty("MQTT_CAPATH");

    if (cafile.empty() && capath.empty()) {
        cafile = PickFirstReadableFile({
            "/etc/ssl/certs/ca-certificates.crt",                // Debian/Ubuntu
            "/etc/pki/tls/certs/ca-bundle.crt",                  // RHEL/CentOS
            "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", // RHEL/Fedora
            "/etc/ssl/cert.pem"                                  // Alpine (часто)
        });
        if (capath.empty()) {
            capath = PickFirstReadableDir({"/etc/ssl/certs",     // Debian/Ubuntu
                                           "/etc/pki/tls/certs", // RHEL/CentOS
                                           "/etc/pki/ca-trust/extracted/pem"});
        }
    }

    if (cafile.empty() && capath.empty()) {
        ERROR_PRINT(
            "TLS: no CA configured. Set MQTT_CAFILE or MQTT_CAPATH (or install system CA bundle).");
        return MOSQ_ERR_INVAL;
    }

    const char *cafile_c = cafile.empty() ? nullptr : cafile.c_str();
    const char *capath_c = capath.empty() ? nullptr : capath.c_str();

    int rc = mosquitto_tls_set(mosq, cafile_c, capath_c, nullptr, nullptr, nullptr);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("mosquitto_tls_set failed: %s", mosquitto_strerror(rc));
        return rc;
    }

    mosquitto_tls_insecure_set(mosq, 0);

    INFO_PRINT("TLS configured. cafile=%s capath=%s", cafile.empty() ? "(null)" : cafile.c_str(),
               capath.empty() ? "(null)" : capath.c_str());
    return MOSQ_ERR_SUCCESS;
}
constexpr int kLwtQos = 1;
constexpr bool kLwtRetain = true;
constexpr const char *kOnlinePayload = "online";
constexpr const char *kOfflinePayload = "offline";
} // namespace

std::shared_ptr<MqttService> MqttService::Create(Args args, std::shared_ptr<Conductor> conductor) {
    return std::make_shared<MqttService>(args, conductor);
}

MqttService::MqttService(Args args, std::shared_ptr<Conductor> conductor)
    : SignalingService(conductor),
      port_(args.mqtt_port),
      uid_(args.uid),
      hostname_(args.mqtt_host),
      username_(args.mqtt_username),
      password_(args.mqtt_password),
      sdp_base_topic_(GetTopic("sdp")),
      ice_base_topic_(GetTopic("ice")),
      status_topic_(GetTopic("status")),
      connection_(nullptr) {}

std::string MqttService::GetTopic(const std::string &topic, const std::string &client_id) const {
    std::string result;
    if (!uid_.empty()) {
        result = uid_ + "/";
    }
    result += topic;
    if (!client_id.empty()) {
        result += "/" + client_id;
    }
    return result;
}

MqttService::~MqttService() { Disconnect(); }

void MqttService::OnRemoteSdp(const std::string &peer_id, const std::string &message) {
    nlohmann::json jsonObj = nlohmann::json::parse(message);
    std::string sdp = jsonObj["sdp"];
    std::string type = jsonObj["type"];
    DEBUG_PRINT("Received remote [%s] SDP: %s", type.c_str(), sdp.c_str());

    auto peer = GetPeer(peer_id);
    if (peer) {
        peer->SetRemoteSdp(sdp, type);
    }
}

void MqttService::OnRemoteIce(const std::string &peer_id, const std::string &message) {
    nlohmann::json jsonObj = nlohmann::json::parse(message);
    std::string sdp_mid = jsonObj["sdpMid"];
    int sdp_mline_index = jsonObj["sdpMLineIndex"];
    std::string candidate = jsonObj["candidate"];
    DEBUG_PRINT("Received remote ICE: %s, %d, %s", sdp_mid.c_str(), sdp_mline_index,
                candidate.c_str());

    auto peer = GetPeer(peer_id);
    if (peer) {
        peer->SetRemoteIce(sdp_mid, sdp_mline_index, candidate);
    }
}

void MqttService::AnswerLocalSdp(const std::string &peer_id, const std::string &sdp,
                                 const std::string &type) {
    DEBUG_PRINT("Answer local [%s] SDP: %s", type.c_str(), sdp.c_str());
    nlohmann::json jsonData;
    jsonData["type"] = type;
    jsonData["sdp"] = sdp;
    std::string jsonString = jsonData.dump();

    Publish(GetTopic("sdp", peer_id_to_client_id_[peer_id]), jsonString);
}

void MqttService::AnswerLocalIce(const std::string &peer_id, const std::string &sdp_mid,
                                 const int sdp_mline_index, const std::string &candidate) {
    DEBUG_PRINT("Sent local ICE:  %s, %d, %s", sdp_mid.c_str(), sdp_mline_index, candidate.c_str());
    nlohmann::json jsonData;
    jsonData["sdpMid"] = sdp_mid;
    jsonData["sdpMLineIndex"] = sdp_mline_index;
    jsonData["candidate"] = candidate;
    std::string jsonString = jsonData.dump();

    Publish(GetTopic("ice", peer_id_to_client_id_[peer_id]), jsonString);
}

void MqttService::Disconnect() {
    if (!connection_) {
        INFO_PRINT("MQTT service already released.");
        mosquitto_lib_cleanup();
        return;
    }
    PublishRetain(status_topic_, kOfflinePayload, kLwtQos, kLwtRetain);
    int rc = mosquitto_loop_stop(connection_, true);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("mosquitto_loop_stop: %s", mosquitto_strerror(rc));
    }

    rc = mosquitto_disconnect(connection_);
    if (rc != MOSQ_ERR_SUCCESS) {
        DEBUG_PRINT("mosquitto_disconnect: %s", mosquitto_strerror(rc));
    }

    mosquitto_destroy(connection_);
    connection_ = nullptr;

    mosquitto_lib_cleanup();
    INFO_PRINT("MQTT service is released.");
}

void MqttService::Publish(const std::string &topic, const std::string &msg) {
    int rc =
        mosquitto_publish(connection_, NULL, topic.c_str(), msg.length(), msg.c_str(), 2, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("Error publishing: %s", mosquitto_strerror(rc));
    }
}

void MqttService::PublishRetain(const std::string& topic, const std::string& msg, int qos, bool retain) {
    int rc = mosquitto_publish(connection_, nullptr, topic.c_str(), static_cast<int>(msg.size()),
                              msg.c_str(), qos, retain);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("Error publishing: %s", mosquitto_strerror(rc));
    }
}

void MqttService::Subscribe(const std::string &topic) {
    int subscribe_result = mosquitto_subscribe_v5(connection_, nullptr, topic.c_str(), 0,
                                                  MQTT_SUB_OPT_NO_LOCAL, nullptr);
    if (subscribe_result == MOSQ_ERR_SUCCESS) {
        DEBUG_PRINT("Successfully subscribed to topic: %s", topic.c_str());
    } else {
        DEBUG_PRINT("Failed to subscribe topic: %s", topic.c_str());
    }
}

void MqttService::Unsubscribe(const std::string &topic) {
    int unsubscribe_result = mosquitto_unsubscribe_v5(connection_, nullptr, topic.c_str(), nullptr);
    if (unsubscribe_result == MOSQ_ERR_SUCCESS) {
        DEBUG_PRINT("Successfully unsubscribed to topic: %s", topic.c_str());
    } else {
        DEBUG_PRINT("Failed to unsubscribe topic: %s", topic.c_str());
    }
}

void MqttService::OnConnect(struct mosquitto *mosq, void *obj, int result) {
    if (result == 0) {
        Subscribe(sdp_base_topic_ + "/+/offer");
        Subscribe(ice_base_topic_ + "/+/offer");
        PublishRetain(status_topic_, kOnlinePayload, kLwtQos, kLwtRetain);
        INFO_PRINT("MQTT service is ready.");
    } else {
        ERROR_PRINT("Connect failed with error code: %d", result);
    }
}

void MqttService::OnMessage(struct mosquitto *mosq, void *obj,
                            const struct mosquitto_message *message) {
    if (!message->payload)
        return;

    std::string topic(message->topic);
    std::string payload(static_cast<char *>(message->payload));

    auto client_id = GetClientId(topic);

    if (topic.starts_with(sdp_base_topic_)) {
        auto peer = CreatePeer();
        if (!peer) {
            ERROR_PRINT("Failed to create peer.");
            return;
        }

        peer->OnLocalSdp(
            [this](const std::string &peer_id, const std::string &sdp, const std::string &type) {
                AnswerLocalSdp(peer_id, sdp, type);
            });
        peer->OnLocalIce([this](const std::string &peer_id, const std::string &sdp_mid,
                                int sdp_mline_index, const std::string &candidate) {
            AnswerLocalIce(peer_id, sdp_mid, sdp_mline_index, candidate);
        });

        client_id_to_peer_id_[client_id] = peer->id();
        peer_id_to_client_id_[peer->id()] = client_id;

        OnRemoteSdp(client_id_to_peer_id_[client_id], payload);
    } else if (topic.starts_with(ice_base_topic_)) {
        OnRemoteIce(client_id_to_peer_id_[client_id], payload);
    }
}

std::string MqttService::GetClientId(std::string &topic) {
    std::string base_topic;
    if (topic.find(sdp_base_topic_) == 0) {
        base_topic = sdp_base_topic_;
    } else if (topic.find(ice_base_topic_) == 0) {
        base_topic = ice_base_topic_;
    } else {
        return "";
    }
    size_t base_length = base_topic.length();
    size_t start_pos = base_length + 1; // skip the trailing "/" of base_topic
    size_t end_pos = topic.find('/', start_pos);

    if (end_pos != std::string::npos) {
        return topic.substr(start_pos, end_pos - start_pos);
    }

    return "";
}

void MqttService::RefreshPeerMap() {
    auto &map = GetPeerMap();
    auto pm_it = map.begin();
    while (pm_it != map.end()) {
        auto peer_id = pm_it->first;
        auto peer = GetPeer(peer_id);

        DEBUG_PRINT("Found peer_id key: %s, connected value: %d", peer_id.c_str(),
                    peer->isConnected());

        if (!peer->isConnected()) {
            auto client_id = peer_id_to_client_id_[peer_id];
            peer_id_to_client_id_.erase(peer_id);
            pm_it = map.erase(pm_it);
            if (client_id_to_peer_id_[client_id] == peer_id) {
                client_id_to_peer_id_.erase(client_id);
            }
            DEBUG_PRINT("(%s) was erased.", peer_id.c_str());
        } else {
            ++pm_it;
        }
    }
}

void MqttService::ConfigureLwt() {
    // Must be called BEFORE connecting.
    const std::string offline = kOfflinePayload;

    int rc = mosquitto_will_set_v5(connection_, status_topic_.c_str(),
                                   static_cast<int>(offline.size()), offline.c_str(),
                                   kLwtQos, kLwtRetain, nullptr);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("Failed to set LWT: %s", mosquitto_strerror(rc));
        return;
    }

    INFO_PRINT("LWT configured: topic=%s payload=%s (qos=%d retain=%d)",
               status_topic_.c_str(), offline.c_str(), kLwtQos, static_cast<int>(kLwtRetain));
}

void MqttService::Connect() {
    mosquitto_lib_init();

    connection_ = mosquitto_new(NULL, true, this);

    if (connection_ == nullptr) {
        ERROR_PRINT("Failed to new mosquitto object.");
        return;
    }

    mosquitto_int_option(connection_, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);

    if (port_ == 8883) {
        int trc = ConfigureTls(connection_);
        if (trc != MOSQ_ERR_SUCCESS) {
            mosquitto_destroy(connection_);
            connection_ = nullptr;
            mosquitto_lib_cleanup();
            return;
        }
    }

    mosquitto_reconnect_delay_set(connection_, 2, 10, true);

    if (!username_.empty()) {
        mosquitto_username_pw_set(connection_, username_.c_str(), password_.c_str());
    }

    /* Configure callbacks. This should be done before connecting ideally. */
    mosquitto_log_callback_set(connection_,
                               [](struct mosquitto *, void *, int level, const char *str) {
                                   switch (level) {
                                       case MOSQ_LOG_DEBUG:
                                           DEBUG_PRINT("mosquitto[%d]: %s", level, str ? str : "");
                                           return;
                                       case MOSQ_LOG_INFO:
                                           INFO_PRINT("mosquitto[%d]: %s", level, str ? str : "");
                                           return;
                                       case MOSQ_LOG_NOTICE:
                                           INFO_PRINT("mosquitto[%d]: %s", level, str ? str : "");
                                           return;
                                       case MOSQ_LOG_WARNING:
                                           ERROR_PRINT("mosquitto[%d]: %s", level, str ? str : "");
                                           return;
                                       case MOSQ_LOG_ERR:
                                           ERROR_PRINT("mosquitto[%d]: %s", level, str ? str : "");
                                           return;
                                       default:
                                           INFO_PRINT("mosquitto[%d]: %s", level, str ? str : "");
                                   }
                               });

    mosquitto_connect_callback_set(connection_, [](struct mosquitto *mosq, void *obj, int result) {
        MqttService *service = static_cast<MqttService *>(obj);
        service->OnConnect(mosq, obj, result);
    });

    mosquitto_disconnect_callback_set(connection_, [](struct mosquitto *mosq, void *obj, int rc) {
        MqttService *service = static_cast<MqttService *>(obj);
        if (rc == 0) {
            INFO_PRINT("MQTT disconnected normally.");
            return;
        }
        INFO_PRINT("MQTT disconnected unexpectedly (rc=%d %s). ", rc, mosquitto_strerror(rc));
        if (service->port_ == 8883) {
            mosquitto_int_option(service->connection_, MOSQ_OPT_TLS_USE_OS_CERTS, 1);
        }
    });

    mosquitto_message_callback_set(connection_, [](struct mosquitto *mosq, void *obj,
                                                   const struct mosquitto_message *message) {
        MqttService *service = static_cast<MqttService *>(obj);
        service->OnMessage(mosq, obj, message);
    });

    ConfigureLwt();

    INFO_PRINT("Trying to connect to MQTT Broker %s:%d", hostname_.c_str(), port_);
    int rc = mosquitto_connect_async(connection_, hostname_.c_str(), port_, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("mosquitto_connect_async failed: %s", mosquitto_strerror(rc));
    }

    rc = mosquitto_loop_start(connection_); // already handle reconnections
    if (rc != MOSQ_ERR_SUCCESS) {
        ERROR_PRINT("mosquitto_loop_start: %s", mosquitto_strerror(rc));
        mosquitto_destroy(connection_);
        connection_ = nullptr;
        mosquitto_lib_cleanup();
        return;
    }
    INFO_PRINT("MQTT loop started; libmosquitto will handle reconnects.");
}
