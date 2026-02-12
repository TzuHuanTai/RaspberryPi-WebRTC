#include "rtc/rtc_peer.h"

#include <chrono>
#include <regex>

#include "rtc/sfu_channel.h"

rtc::scoped_refptr<RtcPeer> RtcPeer::Create(PeerConfig config) {
    return rtc::make_ref_counted<RtcPeer>(std::move(config));
}

RtcPeer::RtcPeer(PeerConfig config)
    : id_(Utils::GenerateUuid()),
      timeout_(config.timeout),
      is_sfu_peer_(config.is_sfu_peer),
      is_publisher_(config.is_publisher),
      has_candidates_in_sdp_(config.has_candidates_in_sdp),
      is_connected_(false),
      is_complete_(false) {}

RtcPeer::~RtcPeer() {
    Terminate();
    DEBUG_PRINT("peer connection (%s) was destroyed!", id_.c_str());
}

void RtcPeer::CreateOffer() {
    if (signaling_state_ == webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer) {
        return;
    }

    peer_connection_->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

void RtcPeer::Terminate() {
    is_connected_.store(false);
    is_complete_.store(true);

    if (peer_timeout_.joinable()) {
        peer_timeout_.join();
    }
    if (sent_sdp_timeout_.joinable()) {
        sent_sdp_timeout_.join();
    }

    on_local_sdp_fn_ = nullptr;
    on_local_ice_fn_ = nullptr;
    if (peer_connection_) {
        peer_connection_->Close();
        peer_connection_ = nullptr;
    }
    modified_desc_.release();

    if (cmd_channel_) {
        cmd_channel_->Terminate();
    }
    if (lossy_channel_) {
        lossy_channel_->Terminate();
    }
    if (reliable_channel_) {
        reliable_channel_->Terminate();
    }
}

std::string RtcPeer::id() const { return id_; }

bool RtcPeer::isSfuPeer() const { return is_sfu_peer_; }

bool RtcPeer::isPublisher() const { return is_publisher_; }

bool RtcPeer::isConnected() const { return is_connected_.load(); }

void RtcPeer::SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj) {
    custom_video_sink_ = std::move(video_sink_obj);
}

void RtcPeer::SetPeer(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer) {
    peer_connection_ = std::move(peer);
}

rtc::scoped_refptr<webrtc::PeerConnectionInterface> RtcPeer::GetPeer() { return peer_connection_; }

std::shared_ptr<RtcChannel> RtcPeer::CreateDataChannel(ChannelMode mode) {
    struct webrtc::DataChannelInit init;
    init.ordered = true;
    init.id = static_cast<int>(mode);
    if (!is_sfu_peer_) {
        init.negotiated = true;
    }
    if (mode == ChannelMode::Lossy) {
        init.maxRetransmits = 0;
    }

    auto label = ChannelModeToString(mode);
    auto result = peer_connection_->CreateDataChannelOrError(label, &init);

    if (!result.ok()) {
        ERROR_PRINT("Failed to create data channel: %s", label.c_str());
        return nullptr;
    }

    auto dc = result.MoveValue();

    std::shared_ptr<RtcChannel> channel =
        is_sfu_peer_ ? SfuChannel::Create(dc) : RtcChannel::Create(dc);

    if (mode == ChannelMode::Command) {
        DEBUG_PRINT("The Command data channel is established successfully.");
        cmd_channel_ = channel;

        cmd_channel_->RegisterHandler(
            protocol::CommandType::DISCONNECT,
            [this](std::shared_ptr<RtcChannel> datachannel, const protocol::Packet &pkt) {
                DEBUG_PRINT("Received DISCONNECT command. Closing peer connection.");
                peer_connection_->Close();
                if (pkt.has_disconnection_request()) {
                    auto request = pkt.disconnection_request();
                    DEBUG_PRINT("Reason: %s",
                                protocol::DisconnectRequest_DisconnectReason_Name(request.reason())
                                    .c_str());
                }
            });
    } else if (mode == ChannelMode::Lossy) {
        DEBUG_PRINT("The Lossy data channel is established successfully.");
        lossy_channel_ = channel;
    } else if (mode == ChannelMode::Reliable) {
        DEBUG_PRINT("The Reliable data channel is established successfully.");
        reliable_channel_ = channel;
    }

    return channel;
}

std::string RtcPeer::RestartIce(std::string ice_ufrag, std::string ice_pwd) {
    std::string remote_sdp;
    peer_connection_->remote_description()->ToString(&remote_sdp);

    // replace all ice_ufrag and ice_pwd in sdp.
    std::regex ufrag_regex(R"(a=ice-ufrag:([^\r\n]+))");
    std::regex pwd_regex(R"(a=ice-pwd:([^\r\n]+))");
    remote_sdp = std::regex_replace(remote_sdp, ufrag_regex, "a=ice-ufrag:" + ice_ufrag);
    remote_sdp = std::regex_replace(remote_sdp, pwd_regex, "a=ice-pwd:" + ice_pwd);
    SetRemoteSdp(remote_sdp, "offer");

    std::string local_sdp;
    peer_connection_->local_description()->ToString(&local_sdp);

    return local_sdp;
}

void RtcPeer::SetOnDataChannelCallback(OnRtcChannelCallback callback) {
    on_data_channel_ = std::move(callback);
}

void RtcPeer::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) {
    signaling_state_ = new_state;
    auto state = webrtc::PeerConnectionInterface::AsString(new_state);
    DEBUG_PRINT("OnSignalingChange => %s", std::string(state).c_str());
    if (new_state == webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer) {
        peer_timeout_ = std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(timeout_));
            if (peer_connection_ && !is_complete_.load() && !is_connected_.load()) {
                DEBUG_PRINT("Connection timeout after kConnecting. Closing connection.");
                peer_connection_->Close();
            }
        });
    }
}

void RtcPeer::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
    DEBUG_PRINT("On remote DataChannel => %s", channel->label().c_str());

    if (!on_data_channel_) {
        return;
    }

    if (channel->label() == ChannelModeToString(ChannelMode::Command)) {
        cmd_channel_ = RtcChannel::Create(channel);
        on_data_channel_(cmd_channel_);
        DEBUG_PRINT("Command data channel is established successfully.");
    } else if (channel->label() == ChannelModeToString(ChannelMode::Lossy)) {
        lossy_channel_ = SfuChannel::Create(channel);
        on_data_channel_(lossy_channel_);
        DEBUG_PRINT("Lossy data channel is established successfully.");
    } else if (channel->label() == ChannelModeToString(ChannelMode::Reliable)) {
        reliable_channel_ = SfuChannel::Create(channel);
        on_data_channel_(reliable_channel_);
        DEBUG_PRINT("Reliable data channel is established successfully.");
    }
}

void RtcPeer::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) {
    auto state = webrtc::PeerConnectionInterface::AsString(new_state);
    DEBUG_PRINT("OnIceGatheringChange => %s", std::string(state).c_str());
}

void RtcPeer::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
    auto state = webrtc::PeerConnectionInterface::AsString(new_state);
    DEBUG_PRINT("OnConnectionChange => %s", std::string(state).c_str());
    if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
        is_connected_.store(true);
        on_local_ice_fn_ = nullptr;
        on_local_sdp_fn_ = nullptr;
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kFailed) {
        is_connected_.store(false);
        peer_connection_->Close();
    } else if (new_state == webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
        is_connected_.store(false);
        is_complete_.store(true);
    }
}

void RtcPeer::OnIceCandidate(const webrtc::IceCandidateInterface *candidate) {
    if (has_candidates_in_sdp_ && modified_desc_) {
        modified_desc_->AddCandidate(candidate);
    }

    if (on_local_ice_fn_) {
        std::string candidate_str;
        candidate->ToString(&candidate_str);
        on_local_ice_fn_(id_, candidate->sdp_mid(), candidate->sdp_mline_index(), candidate_str);
    }
}

void RtcPeer::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    if (transceiver->receiver()->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO &&
        custom_video_sink_) {
        auto track = transceiver->receiver()->track();
        auto remote_video_track = static_cast<webrtc::VideoTrackInterface *>(track.get());
        DEBUG_PRINT("OnTrack => custom sink(%s) is added!", track->id().c_str());
        remote_video_track->AddOrUpdateSink(custom_video_sink_, rtc::VideoSinkWants());
    }
}

void RtcPeer::OnSuccess(webrtc::SessionDescriptionInterface *desc) {
    std::string sdp;
    desc->ToString(&sdp);

    /* An in-bound DataChannel created by the server side will not connect if the SDP is set to
     * passive. */
    // modified_sdp_ = ModifySetupAttribute(sdp, "passive");
    modified_sdp_ = sdp;

    modified_desc_ =
        webrtc::CreateSessionDescription(desc->GetType(), modified_sdp_, modified_desc_error_);
    if (!modified_desc_) {
        ERROR_PRINT("Failed to create session description: %s",
                    modified_desc_error_->description.c_str());
        return;
    }

    peer_connection_->SetLocalDescription(SetSessionDescription::Create(nullptr, nullptr).get(),
                                          modified_desc_.get());

    if (has_candidates_in_sdp_) {
        EmitLocalSdp(1);
    } else {
        EmitLocalSdp();
    }
}

void RtcPeer::EmitLocalSdp(int delay_sec) {
    if (!on_local_sdp_fn_) {
        return;
    }

    if (sent_sdp_timeout_.joinable()) {
        sent_sdp_timeout_.join();
    }

    auto send_sdp = [this]() {
        std::string type = webrtc::SdpTypeToString(modified_desc_->GetType());
        modified_desc_->ToString(&modified_sdp_);
        on_local_sdp_fn_(id_, modified_sdp_, type);
        on_local_sdp_fn_ = nullptr;
    };

    if (delay_sec > 0) {
        sent_sdp_timeout_ = std::thread([this, send_sdp, delay_sec]() {
            std::this_thread::sleep_for(std::chrono::seconds(delay_sec));
            send_sdp();
        });
    } else {
        send_sdp();
    }
}

void RtcPeer::OnFailure(webrtc::RTCError error) {
    auto type = ToString(error.type());
    ERROR_PRINT("%s; %s", std::string(type).c_str(), error.message());
}

void RtcPeer::SetRemoteSdp(const std::string &sdp, const std::string &sdp_type) {
    if (is_connected_.load()) {
        return;
    }

    absl::optional<webrtc::SdpType> type_maybe = webrtc::SdpTypeFromString(sdp_type);
    if (!type_maybe) {
        ERROR_PRINT("Unknown SDP type: %s", sdp_type.c_str());
        return;
    }
    webrtc::SdpType type = *type_maybe;

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
    if (!session_description) {
        ERROR_PRINT("Can't parse received session description message. %s",
                    error.description.c_str());
        return;
    }

    peer_connection_->SetRemoteDescription(SetSessionDescription::Create(nullptr, nullptr).get(),
                                           session_description.release());

    if (type == webrtc::SdpType::kOffer) {
        peer_connection_->CreateAnswer(this,
                                       webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }
}

void RtcPeer::SetRemoteIce(const std::string &sdp_mid, int sdp_mline_index,
                           const std::string &candidate) {
    if (is_connected_.load()) {
        return;
    }

    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::IceCandidateInterface> ice(
        webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, &error));
    if (!ice.get()) {
        ERROR_PRINT("Can't parse received candidate message. %s", error.description.c_str());
        return;
    }

    if (!peer_connection_->AddIceCandidate(ice.get())) {
        ERROR_PRINT("Failed to apply the received candidate!");
        return;
    }
}

std::string RtcPeer::ModifySetupAttribute(const std::string &sdp, const std::string &new_setup) {
    std::string modified_sdp = sdp;
    const std::string target = "a=setup:";
    size_t pos = 0;

    while ((pos = modified_sdp.find(target, pos)) != std::string::npos) {
        size_t end_pos = modified_sdp.find("\r\n", pos);
        if (end_pos != std::string::npos) {
            modified_sdp.replace(pos, end_pos - pos, target + new_setup);
            pos = end_pos;
        } else {
            break;
        }
    }

    return modified_sdp;
}
