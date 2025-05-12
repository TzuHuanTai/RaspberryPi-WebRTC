#ifndef RTC_PEER_H_
#define RTC_PEER_H_

#include <atomic>
#include <thread>

#include <api/data_channel_interface.h>
#include <api/peer_connection_interface.h>
#include <api/video/video_sink_interface.h>

#include "args.h"
#include "common/logging.h"
#include "rtc/data_channel_subject.h"

enum ChannelMode {
    Command,
    Lossy,
    Reliable
};

static inline std::string ChannelModeToString(ChannelMode id) {
    switch (id) {
        case Command:
            return "cmd_channel";
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

    static rtc::scoped_refptr<SetSessionDescription> Create(OnSuccessFunc on_success,
                                                            OnFailureFunc on_failure) {
        return rtc::make_ref_counted<SetSessionDescription>(std::move(on_success),
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
    static rtc::scoped_refptr<RtcPeer> Create(PeerConfig config);

    RtcPeer(PeerConfig config);
    ~RtcPeer();
    void CreateOffer();
    void Terminate();
    bool IsConnected() const;
    std::string GetId() const;
    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    void SetPeer(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer);
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer();
    std::shared_ptr<DataChannelSubject> CreateDataChannel(ChannelMode mode);
    std::string RestartIce(std::string ice_ufrag, std::string ice_pwd);

    // SignalingMessageObserver implementation.
    void SetRemoteSdp(const std::string &sdp, const std::string &type) override;
    void SetRemoteIce(const std::string &sdp_mid, int sdp_mline_index,
                      const std::string &candidate) override;

  private:
    // PeerConnectionObserver implementation.
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override;
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
    void
    OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
    void
    OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;
    void OnIceCandidate(const webrtc::IceCandidateInterface *candidate) override;
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface *desc) override;
    void OnFailure(webrtc::RTCError error) override;

    std::string ModifySetupAttribute(const std::string &sdp, const std::string &new_setup);
    void EmitLocalSdp(int delay_sec = 0);

    std::string id_;
    PeerConfig config_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> is_complete_;
    std::thread peer_timeout_;
    std::thread sent_sdp_timeout_;

    std::string modified_sdp_;
    webrtc::SdpParseError *modified_desc_error_;
    webrtc::PeerConnectionInterface::SignalingState signaling_state_;
    std::unique_ptr<webrtc::SessionDescriptionInterface> modified_desc_;

    std::shared_ptr<DataChannelSubject> cmd_channel_;
    std::shared_ptr<DataChannelSubject> lossy_channel_;
    std::shared_ptr<DataChannelSubject> reliable_channel_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    rtc::VideoSinkInterface<webrtc::VideoFrame> *custom_video_sink_;
};

#endif // RTC_PEER_H_
