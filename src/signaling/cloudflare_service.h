#ifndef CLOUDFLARE_SERVICE_H_
#define CLOUDFLARE_SERVICE_H_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "args.h"
#include "rtc/conductor.h"
#include "signaling/http_client.h"
#include "signaling/signaling_service.h"

// Publishes the local tracks into a Cloudflare Realtime SFU through the picamera device API.
//
// The device never holds the Realtime App ID or App Secret: every call goes to
// <api-url>/sfu/..., which attaches them and forwards the request. That is what makes SFU
// streaming usable without a Cloudflare account of your own.
//
// There is no signaling socket either way — the whole exchange is a handful of one-shot HTTPS
// requests, the device is always the offerer, and there is no trickle-ICE endpoint, so the
// candidates have to ride inside the offer. With no socket to watch, a dropped link is only
// visible through the PeerConnection state, which is why this service polls the peer instead of
// reacting to a read error.
class CloudflareService : public SignalingService,
                          public std::enable_shared_from_this<CloudflareService> {
  public:
    static std::shared_ptr<CloudflareService>
    Create(Args args, std::shared_ptr<Conductor> conductor, boost::asio::io_context &ioc);
    CloudflareService(Args args, std::shared_ptr<Conductor> conductor,
                      boost::asio::io_context &ioc);
    ~CloudflareService() override;

    void Connect() override;
    void Disconnect() override;

  private:
    struct LocalTrack {
        std::string mid;
        std::string name;
    };

    std::shared_ptr<Conductor> conductor_;
    Args args_;
    boost::asio::io_context &ioc_;
    boost::asio::steady_timer reconnect_timer_, health_timer_, status_timer_, report_timer_;
    webrtc::scoped_refptr<RtcPeer> peer_;

    std::string base_url_;
    std::string report_url_;
    std::string session_id_;
    // The track names the SFU accepted, kept so the periodic re-report does not have to walk the
    // transceivers again from the io thread.
    std::vector<std::string> track_names_;
    int report_retry_count_ = 0;
    int retry_count_ = 0;
    bool stopped_ = false;
    bool reconnect_pending_ = false;
    bool peer_was_connected_ = false;
    std::chrono::steady_clock::time_point peer_down_since_;
    // Bumped on every (re)connect attempt so that handlers belonging to a torn-down session
    // can be discarded instead of interfering with the new one.
    uint64_t generation_ = 0;

    // connection lifecycle
    void StartSession();
    void CreateSession(uint64_t gen);
    void CreatePeerAndOffer(uint64_t gen);
    void ScheduleReconnect(const std::string &reason);
    void CleanupSession();

    // signaling
    void PublishTracks(uint64_t gen, const std::string &offer_sdp,
                       const std::vector<LocalTrack> &tracks);
    void Renegotiate(uint64_t gen, const std::string &answer_sdp);
    std::vector<LocalTrack> CollectLocalTracks() const;

    // liveness
    void MonitorPeer(uint64_t gen);
    void PollSessionStatus(uint64_t gen);

    // session registry
    void ReportSession(uint64_t gen);
    void ScheduleSessionReport(uint64_t gen, int delay_sec);

    void Request(uint64_t gen, boost::beast::http::verb method, const std::string &path,
                 const std::string &body,
                 std::function<void(const HttpClient::Response &)> on_complete);
};

#endif // CLOUDFLARE_SERVICE_H_
