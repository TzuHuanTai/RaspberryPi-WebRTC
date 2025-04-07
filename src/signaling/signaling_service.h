#ifndef SIGNALING_SERVICE_H_
#define SIGNALING_SERVICE_H_

#include "common/logging.h"
#include "common/worker.h"
#include "conductor.h"

class SignalingService {
  public:
    SignalingService(std::shared_ptr<Conductor> conductor, bool has_candidates_in_sdp = false)
        : conductor(conductor) {}

    void Start() {
        worker_ = std::make_unique<Worker>("cleaner", [this]() {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            RefreshPeerMap();
        });
        worker_->Run();
        Connect();
    }

    rtc::scoped_refptr<RtcPeer> CreatePeer(PeerConfig config = PeerConfig{}) {
        if (!conductor) {
            ERROR_PRINT("Conductor is not initialized.");
            return nullptr;
        }

        auto peer = conductor->CreatePeerConnection(config);
        if (!config.is_sfu_peer) {
            peer_map_[peer->GetId()] = peer;
        }
        return peer;
    }

    rtc::scoped_refptr<RtcPeer> GetPeer(const std::string &peer_id) {
        auto it = peer_map_.find(peer_id);
        if (it != peer_map_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void RemovePeerFromMap(const std::string &peer_id) { peer_map_.erase(peer_id); }

  protected:
    std::unordered_map<std::string, rtc::scoped_refptr<RtcPeer>> &GetPeerMap() { return peer_map_; }

    virtual void RefreshPeerMap() {
        auto pm_it = peer_map_.begin();
        while (pm_it != peer_map_.end()) {
            auto peer_id = pm_it->second->GetId();

            if (pm_it->second && !pm_it->second->IsConnected()) {
                pm_it = peer_map_.erase(pm_it);
                DEBUG_PRINT("peer_map (%s) was erased.", peer_id.c_str());
            } else {
                ++pm_it;
            }
        }
    }
    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

    std::shared_ptr<Conductor> conductor;

  private:
    std::unique_ptr<Worker> worker_;
    std::unordered_map<std::string, rtc::scoped_refptr<RtcPeer>> peer_map_;
};

#endif
