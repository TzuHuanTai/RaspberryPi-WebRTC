#include "rtc/conductor.h"

#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/rtc_event_log/rtc_event_log_factory.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/video_codecs/video_decoder_factory.h>
#include <api/video_codecs/video_decoder_factory_template.h>
#include <api/video_codecs/video_decoder_factory_template_dav1d_adapter.h>
#include <api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h>
#include <api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h>
#include <api/video_codecs/video_decoder_factory_template_open_h264_adapter.h>
#include <media/engine/webrtc_media_engine.h>
#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_device/include/audio_device_factory.h>
#include <modules/audio_device/linux/audio_device_alsa_linux.h>
#include <modules/audio_device/linux/audio_device_pulse_linux.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <pc/video_track_source_proxy.h>
#include <rtc_base/ssl_adapter.h>

#include "capturer/libcamera_capturer.h"
#include "capturer/v4l2_capturer.h"
#include "common/logging.h"
#include "common/utils.h"
#include "customized_video_encoder_factory.h"
#include "track/v4l2dma_track_source.h"

std::shared_ptr<Conductor> Conductor::Create(Args args) {
    auto ptr = std::make_shared<Conductor>(args);
    ptr->InitializePeerConnectionFactory();
    ptr->InitializeTracks();
    ptr->InitializeIpcServer();
    return ptr;
}

Conductor::Conductor(Args args)
    : args(args) {}

Conductor::~Conductor() {
    if (ipc_server_) {
        ipc_server_->Stop();
    }
    audio_track_ = nullptr;
    video_track_ = nullptr;
    video_capture_source_ = nullptr;
    peer_connection_factory_ = nullptr;
    rtc::CleanupSSL();
}

Args Conductor::config() const { return args; }

std::shared_ptr<PaCapturer> Conductor::AudioSource() const { return audio_capture_source_; }

std::shared_ptr<VideoCapturer> Conductor::VideoSource() const { return video_capture_source_; }

void Conductor::InitializeTracks() {
    if (audio_track_ == nullptr && !args.no_audio) {
        audio_capture_source_ = PaCapturer::Create(args);
        auto options = peer_connection_factory_->CreateAudioSource(cricket::AudioOptions());
        audio_track_ = peer_connection_factory_->CreateAudioTrack("audio_track", options.get());
    }

    if (video_track_ == nullptr && !args.camera.empty()) {
        video_capture_source_ = ([this]() -> std::shared_ptr<VideoCapturer> {
            if (args.use_libcamera) {
                return LibcameraCapturer::Create(args);
            } else {
                return V4L2Capturer::Create(args);
            }
        })();

        video_track_source_ = ([this]() -> rtc::scoped_refptr<ScaleTrackSource> {
            if (args.hw_accel) {
                return V4L2DmaTrackSource::Create(video_capture_source_);
            } else {
                return ScaleTrackSource::Create(video_capture_source_);
            }
        })();

        auto video_source = webrtc::VideoTrackSourceProxy::Create(
            signaling_thread_.get(), worker_thread_.get(), video_track_source_);
        video_track_ = peer_connection_factory_->CreateVideoTrack(video_source, "video_track");
    }
}

void Conductor::AddTracks(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
    if (!peer_connection->GetSenders().empty()) {
        DEBUG_PRINT("Already add tracks.");
        return;
    }

    if (audio_track_) {
        auto audio_res = peer_connection->AddTrack(audio_track_, {args.uid});
        if (!audio_res.ok()) {
            ERROR_PRINT("Failed to add audio track, %s", audio_res.error().message());
        }
    }

    if (video_track_) {
        auto video_res = peer_connection->AddTrack(video_track_, {args.uid});
        if (!video_res.ok()) {
            ERROR_PRINT("Failed to add video track, %s", video_res.error().message());
        }

        auto video_sender_ = video_res.value();
        webrtc::RtpParameters parameters = video_sender_->GetParameters();
        parameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
        video_sender_->SetParameters(parameters);
    }
}

rtc::scoped_refptr<RtcPeer> Conductor::CreatePeerConnection(PeerConfig config) {
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = args.stun_url;
    config.servers.push_back(server);

    if (!args.turn_url.empty()) {
        webrtc::PeerConnectionInterface::IceServer turn_server;
        turn_server.uri = args.turn_url;
        turn_server.username = args.turn_username;
        turn_server.password = args.turn_password;
        config.servers.push_back(turn_server);
    }

    config.timeout = args.peer_timeout;
    auto peer = RtcPeer::Create(config);
    auto result = peer_connection_factory_->CreatePeerConnectionOrError(
        config, webrtc::PeerConnectionDependencies(peer.get()));

    if (!result.ok()) {
        DEBUG_PRINT("Peer connection is failed to create!");
        return nullptr;
    }

    peer->SetPeer(result.MoveValue());

    InitializeDataChannels(peer);

    AddTracks(peer->GetPeer());

    DEBUG_PRINT("Peer connection(%s) is created! ", peer->id().c_str());
    return peer;
}

void Conductor::InitializeDataChannels(rtc::scoped_refptr<RtcPeer> peer) {
    if (peer->isSfuPeer() && !peer->isPublisher()) {
        peer->SetOnDataChannelCallback([this](std::shared_ptr<RtcChannel> channel) {
            DEBUG_PRINT("Remote channel (%s) from sfu subscriber peer [%s]",
                        channel->label().c_str(), channel->id().c_str());
            BindDataChannelToIpcReceiver(channel);
        });
        return;
    }

    // Essential data channels（Lossy / Reliable / Command）
    auto lossy_channel = peer->CreateDataChannel(ChannelMode::Lossy);
    auto reliable_channel = peer->CreateDataChannel(ChannelMode::Reliable);

    if (args.enable_ipc) {
        switch (args.ipc_channel_mode) {
            case ChannelMode::Lossy:
                BindIpcToDataChannel(lossy_channel);
                break;
            case ChannelMode::Reliable:
                BindIpcToDataChannel(reliable_channel);
                break;
            default:
                BindIpcToDataChannel(lossy_channel);
                BindIpcToDataChannel(reliable_channel);
                break;
        }
    }

    if (!peer->isSfuPeer()) {
        InitializeCommandChannel(peer);
    }
}

void Conductor::InitializeCommandChannel(rtc::scoped_refptr<RtcPeer> peer) {
    auto cmd_channel = peer->CreateDataChannel(ChannelMode::Command);
    cmd_channel->RegisterHandler(
        CommandType::SNAPSHOT,
        [this](std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
            OnSnapshot(datachannel, msg);
        });
    cmd_channel->RegisterHandler(
        CommandType::METADATA,
        [this](std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
            OnMetadata(datachannel, msg);
        });
    cmd_channel->RegisterHandler(
        CommandType::RECORDING,
        [this](std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
            OnRecord(datachannel, msg);
        });
    cmd_channel->RegisterHandler(
        CommandType::CAMERA_OPTION,
        [this](std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
            OnCameraOption(datachannel, msg);
        });
}

void Conductor::OnSnapshot(std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
    try {
        std::stringstream ss(msg);
        int num;
        ss >> num;
        int quality = ss.fail() ? 100 : num;

        auto i420buff = video_capture_source_->GetI420Frame();
        auto jpg_buffer =
            Utils::ConvertYuvToJpeg(i420buff->DataY(), args.width, args.height, quality);
        datachannel->Send(std::move(jpg_buffer));
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
}

void Conductor::OnMetadata(std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
    DEBUG_PRINT("OnMetadata msg: %s", msg.c_str());
    json jsonObj = json::parse(msg.c_str());

    MetadataCommand cmd = jsonObj["command"];
    std::string message = jsonObj["message"];
    DEBUG_PRINT("parse meta cmd message => %hhu, %s", cmd, message.c_str());

    if (args.record_path.empty()) {
        return;
    }

    if ((cmd == MetadataCommand::LATEST) || (cmd == MetadataCommand::OLDER && message.empty())) {
        auto latest_mp4_path = Utils::FindSecondNewestFile(args.record_path, ".mp4");
        DEBUG_PRINT("LATEST: %s", latest_mp4_path.c_str());
        MetaMessage metadata(latest_mp4_path);
        datachannel->Send(metadata);
    } else if (cmd == MetadataCommand::OLDER) {
        auto paths = Utils::FindOlderFiles(message, 8);
        for (auto &path : paths) {
            DEBUG_PRINT("OLDER: %s", path.c_str());
            MetaMessage metadata(path);
            datachannel->Send(metadata);
        }
    } else if (cmd == MetadataCommand::SPECIFIC_TIME) {
        auto path = Utils::FindFilesFromDatetime(args.record_path, message);
        MetaMessage metadata(path);
        datachannel->Send(metadata);
    }
}

void Conductor::OnRecord(std::shared_ptr<RtcChannel> datachannel, const std::string &path) {
    if (args.record_path.empty()) {
        return;
    }
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            ERROR_PRINT("Unable to open file: %s", path.c_str());
            return;
        }
        datachannel->Send(file);
        DEBUG_PRINT("Sent Video: %s", path.c_str());
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
}

void Conductor::OnCameraOption(std::shared_ptr<RtcChannel> datachannel, const std::string &msg) {
    DEBUG_PRINT("OnCameraControl msg: %s", msg.c_str());
    json jsonObj = json::parse(msg.c_str());

    int key = jsonObj["key"];
    int value = jsonObj["value"];
    DEBUG_PRINT("parse meta cmd message => %d, %d", key, value);

    try {
        if (!args.use_libcamera) {
            throw std::runtime_error("Setting camera options only valid with libcamera.");
        }
        video_capture_source_->SetControls(key, value);
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
}

void Conductor::InitializePeerConnectionFactory() {
    rtc::InitializeSSL();

    network_thread_ = rtc::Thread::CreateWithSocketServer();
    worker_thread_ = rtc::Thread::Create();
    signaling_thread_ = rtc::Thread::Create();

    if (network_thread_->Start()) {
        DEBUG_PRINT("network thread start: success!");
    }
    if (worker_thread_->Start()) {
        DEBUG_PRINT("worker thread start: success!");
    }
    if (signaling_thread_->Start()) {
        DEBUG_PRINT("signaling thread start: success!");
    }

    webrtc::PeerConnectionFactoryDependencies dependencies;
    dependencies.network_thread = network_thread_.get();
    dependencies.worker_thread = worker_thread_.get();
    dependencies.signaling_thread = signaling_thread_.get();
    dependencies.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
    dependencies.call_factory = webrtc::CreateCallFactory();
    dependencies.event_log_factory =
        std::make_unique<webrtc::RtcEventLogFactory>(dependencies.task_queue_factory.get());
    dependencies.trials = std::make_unique<webrtc::FieldTrialBasedConfig>();

    cricket::MediaEngineDependencies media_dependencies;
    media_dependencies.task_queue_factory = dependencies.task_queue_factory.get();

    webrtc::AudioDeviceModule::AudioLayer audio_layer = webrtc::AudioDeviceModule::kLinuxPulseAudio;
    if (args.no_audio) {
        audio_layer = webrtc::AudioDeviceModule::kDummyAudio;
    }
    media_dependencies.adm =
        webrtc::AudioDeviceModule::Create(audio_layer, dependencies.task_queue_factory.get());
    if (media_dependencies.adm->Init() != 0) {
        ERROR_PRINT("Failed to initialize AudioDeviceModule.\n"
                    "If your system does not have PulseAudio installed, please either:\n"
                    "   - Install PulseAudio, or\n"
                    "   - Run with `--no-audio` to disable audio support.\n");
        std::exit(EXIT_FAILURE);
    }
    media_dependencies.audio_encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
    media_dependencies.audio_decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
    media_dependencies.audio_processing = webrtc::AudioProcessingBuilder().Create();
    media_dependencies.audio_mixer = nullptr;
    media_dependencies.video_encoder_factory = CreateCustomizedVideoEncoderFactory(args);
    media_dependencies.video_decoder_factory = std::make_unique<webrtc::VideoDecoderFactoryTemplate<
        webrtc::OpenH264DecoderTemplateAdapter, webrtc::LibvpxVp8DecoderTemplateAdapter,
        webrtc::LibvpxVp9DecoderTemplateAdapter, webrtc::Dav1dDecoderTemplateAdapter>>();
    media_dependencies.trials = dependencies.trials.get();
    dependencies.media_engine = cricket::CreateMediaEngine(std::move(media_dependencies));

    peer_connection_factory_ = CreateModularPeerConnectionFactory(std::move(dependencies));
}

void Conductor::InitializeIpcServer() {
    if (args.enable_ipc) {
        ipc_server_ = UnixSocketServer::Create(args.socket_path);
        ipc_server_->Start();
    }
}

void Conductor::BindIpcToDataChannel(std::shared_ptr<RtcChannel> channel) {
    BindIpcToDataChannelSender(channel);
    BindDataChannelToIpcReceiver(channel);
}

void Conductor::BindIpcToDataChannelSender(std::shared_ptr<RtcChannel> channel) {
    if (!channel || !ipc_server_) {
        ERROR_PRINT("IPC or DataChannel is not found!");
        return;
    }

    const auto id = channel->id();
    const auto label = channel->label();

    ipc_server_->RegisterPeerCallback(id, [channel](const std::string &msg) {
        channel->Send(msg);
    });
    DEBUG_PRINT("[%s] DataChannel (%s) registered to IPC server for sending.", id.c_str(),
                label.c_str());

    channel->OnClosed([this, id, label]() {
        ipc_server_->UnregisterPeerCallback(id);
        DEBUG_PRINT("[%s] DataChannel (%s) unregistered from IPC server.", id.c_str(),
                    label.c_str());
    });
}

void Conductor::BindDataChannelToIpcReceiver(std::shared_ptr<RtcChannel> channel) {
    if (!channel || !ipc_server_)
        return;

    channel->RegisterHandler(CommandType::CUSTOM, [this](const std::string &msg) {
        ipc_server_->Write(msg);
    });
    DEBUG_PRINT("DataChannel (%s) connected to IPC server for receiving.",
                channel->label().c_str());
}
