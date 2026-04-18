#ifndef RTC_PEER_H_
#define RTC_PEER_H_

#include <atomic>

#include <api/data_channel_interface.h>
#include <api/peer_connection_interface.h>
#include <api/task_queue/pending_task_safety_flag.h>
#include <api/video/video_sink_interface.h>

#include "args.h"
#include "common/logging.h"
#include "rtc/rtc_channel.h"

enum ChannelMode {
    Command,
    Lossy,
    Reliable
};

static inline std::string ChannelModeToString(ChannelMode id) {
    switch (id) {
        case Command:
            return "command";
        case Lossy:
            return "_lossy";
        case Reliable:
            return "_reliable";
        default:
            return "unknown";
    }
}

struct PeerConfig : public webrtc::PeerConnectionInterface::RTCConfiguration {
    int timeout = 10;
    bool is_publisher = true;
    bool is_sfu_peer = false;
    bool has_candidates_in_sdp = false;
};

class SetSessionDescription : public webrtc::SetSessionDescriptionObserver {
  public:
    typedef std::function<void()> OnSuccessFunc;
    typedef std::function<void(webrtc::RTCError)> OnFailureFunc;

    SetSessionDescription(OnSuccessFunc on_success, OnFailureFunc on_failure)
        : on_success_(std::move(on_success)),
          on_failure_(std::move(on_failure)) {}

    static webrtc::scoped_refptr<SetSessionDescription> Create(OnSuccessFunc on_success,
                                                               OnFailureFunc on_failure) {
        return webrtc::make_ref_counted<SetSessionDescription>(std::move(on_success),
                                                               std::move(on_failure));
    }

  protected:
    void OnSuccess() override {
        INFO_PRINT("=> Set sdp success!");
        auto f = std::move(on_success_);
        if (f) {
            f();
        }
    }
    void OnFailure(webrtc::RTCError error) override {
        INFO_PRINT("=> Set sdp failed: %s", error.message());
        auto f = std::move(on_failure_);
        if (f) {
            f(error);
        }
    }

    OnSuccessFunc on_success_;
    OnFailureFunc on_failure_;
};

class SignalingMessageObserver {
  public:
    using OnLocalSdpFunc = std::function<void(const std::string &peer_id, const std::string &sdp,
                                              const std::string &type)>;
    using OnLocalIceFunc =
        std::function<void(const std::string &peer_id, const std::string &sdp_mid,
                           int sdp_mline_index, const std::string &candidate)>;

    virtual void SetRemoteSdp(const std::string &sdp, const std::string &type) = 0;
    virtual void SetRemoteIce(const std::string &sdp_mid, int sdp_mline_index,
                              const std::string &candidate) = 0;

    void OnLocalSdp(OnLocalSdpFunc func) { on_local_sdp_fn_ = std::move(func); };
    void OnLocalIce(OnLocalIceFunc func) { on_local_ice_fn_ = std::move(func); };

  protected:
    OnLocalSdpFunc on_local_sdp_fn_ = nullptr;
    OnLocalIceFunc on_local_ice_fn_ = nullptr;
};

class RtcPeer : public webrtc::PeerConnectionObserver,
                public webrtc::CreateSessionDescriptionObserver,
                public SignalingMessageObserver {
  public:
    using OnRtcChannelCallback = std::function<void(std::shared_ptr<RtcChannel>)>;

    static webrtc::scoped_refptr<RtcPeer> Create(PeerConfig config);

    RtcPeer(PeerConfig config);
    ~RtcPeer();
    void CreateOffer();
    void Terminate();

    bool isSfuPeer() const;
    bool isPublisher() const;
    bool isConnected() const;
    std::string id() const;

    void SetSink(webrtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    void SetPeer(webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer);
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer();
    std::shared_ptr<RtcChannel> CreateDataChannel(ChannelMode mode);
    std::string RestartIce(std::string ice_ufrag, std::string ice_pwd);
    void SetOnDataChannelCallback(OnRtcChannelCallback callback);

    // SignalingMessageObserver implementation.
    void SetRemoteSdp(const std::string &sdp, const std::string &type) override;
    void SetRemoteIce(const std::string &sdp_mid, int sdp_mline_index,
                      const std::string &candidate) override;

  private:
    // PeerConnectionObserver implementation.
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
    void OnDataChannel(webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    void
    OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void
    OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    void OnRenegotiationNeeded() override;
    void OnTrack(webrtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
    void OnFailure(webrtc::RTCError error) override;

    std::string ModifySetupAttribute(const std::string &sdp, const std::string &new_setup);
    void EmitLocalSdp(int delay_sec = 0);

    int timeout_;
    std::string id_;
    bool is_sfu_peer_;
    bool is_publisher_;
    bool has_candidates_in_sdp_;
    bool needs_renegotiation_ = false;
    std::atomic<bool> is_connected_ = false;
    std::atomic<bool> is_complete_ = false;
    std::atomic<bool> is_negotiating_ = false;
    webrtc::ScopedTaskSafetyDetached peer_timeout_safety_;
    webrtc::ScopedTaskSafetyDetached sdp_emit_safety_;

    std::string modified_sdp_;
    webrtc::PeerConnectionInterface::SignalingState signaling_state_;
    std::unique_ptr<webrtc::SessionDescriptionInterface> modified_desc_;

    OnRtcChannelCallback on_data_channel_;
    std::shared_ptr<RtcChannel> cmd_channel_;
    std::shared_ptr<RtcChannel> lossy_channel_;
    std::shared_ptr<RtcChannel> reliable_channel_;
    webrtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    webrtc::VideoSinkInterface<webrtc::VideoFrame> *custom_video_sink_;
};

#endif // RTC_PEER_H_
