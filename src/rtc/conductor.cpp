#include "rtc/conductor.h"

#include <algorithm>

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

#if defined(USE_LIBCAMERA_CAPTURE)
#include "capturer/libcamera_capturer.h"
#elif defined(USE_LIBARGUS_CAPTURE)
// #include "capturer/libargus_buffer_capturer.h"
#include "capturer/libargus_egl_capturer.h"
#endif
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
            if (!args.use_libcamera && !args.use_libargus) {
                INFO_PRINT("Use v4l2 capturer.");
                return V4L2Capturer::Create(args);
            }
#if defined(USE_LIBCAMERA_CAPTURE)
            else if (args.use_libcamera) {
                INFO_PRINT("Use libcamera capturer.");
                return LibcameraCapturer::Create(args);
            }
#elif defined(USE_LIBARGUS_CAPTURE)
            else if (args.use_libargus) {
                INFO_PRINT("Use libargus capturer.");
                // return LibargusBufferCapturer::Create(args);
                return LibargusEglCapturer::Create(args);
            }
#endif
            ERROR_PRINT("Capturer is undefined.");
            return nullptr;
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

    if (args.enable_ipc) {
        switch (args.ipc_channel_mode) {
            case ChannelMode::Lossy: {
                auto lossy_channel = peer->CreateDataChannel(ChannelMode::Lossy);
                BindIpcToDataChannel(lossy_channel);
                break;
            }
            case ChannelMode::Reliable: {
                auto reliable_channel = peer->CreateDataChannel(ChannelMode::Reliable);
                BindIpcToDataChannel(reliable_channel);
                break;
            }
            default: {
                auto lossy_channel = peer->CreateDataChannel(ChannelMode::Lossy);
                auto reliable_channel = peer->CreateDataChannel(ChannelMode::Reliable);
                BindIpcToDataChannel(lossy_channel);
                BindIpcToDataChannel(reliable_channel);
                break;
            }
        }
    }

    if (!peer->isSfuPeer()) {
        InitializeCommandChannel(peer);
    }
}

void Conductor::InitializeCommandChannel(rtc::scoped_refptr<RtcPeer> peer) {
    auto cmd_channel = peer->CreateDataChannel(ChannelMode::Command);
    cmd_channel->RegisterHandler(
        protocol::CommandType::TAKE_SNAPSHOT,
        [this](std::shared_ptr<RtcChannel> datachannel, const protocol::Packet pkt) {
            TakeSnapshot(datachannel, pkt);
        });
    cmd_channel->RegisterHandler(
        protocol::CommandType::QUERY_FILE,
        [this](std::shared_ptr<RtcChannel> datachannel, const protocol::Packet pkt) {
            QueryFile(datachannel, pkt);
        });
    cmd_channel->RegisterHandler(
        protocol::CommandType::TRANSFER_FILE,
        [this](std::shared_ptr<RtcChannel> datachannel, const protocol::Packet pkt) {
            TransferFile(datachannel, pkt);
        });
    cmd_channel->RegisterHandler(
        protocol::CommandType::CONTROL_CAMERA,
        [this](std::shared_ptr<RtcChannel> datachannel, const protocol::Packet pkt) {
            ControlCamera(datachannel, pkt);
        });
}

void Conductor::TakeSnapshot(std::shared_ptr<RtcChannel> datachannel, const protocol::Packet &pkt) {
    try {
        auto quality = std::clamp(pkt.take_snapshot_request().quality(), 0u, 100u);

        auto i420buff = video_capture_source_->GetI420Frame(args.live_stream_idx);
        auto jpg_buffer = Utils::ConvertYuvToJpeg(
            i420buff->DataY(), video_capture_source_->width(args.live_stream_idx),
            video_capture_source_->height(args.live_stream_idx), quality);
        datachannel->Send(std::move(jpg_buffer));
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
}

void Conductor::QueryFile(std::shared_ptr<RtcChannel> datachannel, const protocol::Packet &pkt) {
    if (!pkt.has_query_file_request()) {
        ERROR_PRINT("Invalid metadata request");
        return;
    }

    if (args.record_path.empty()) {
        ERROR_PRINT("Recording path is not set, unable to query files.");
        return;
    }

    auto req = pkt.query_file_request();
    auto type = req.type();
    const std::string &parameter = req.parameter();

    if (type == protocol::QueryFileType::LATEST_FILE || parameter.empty()) {
        auto path = Utils::FindSecondNewestFile(args.record_path, ".mp4");
        DEBUG_PRINT("LATEST: %s", path.c_str());
        SendFileResponse(datachannel, path);
    } else if (type == protocol::QueryFileType::BEFORE_FILE) {
        auto paths = Utils::FindOlderFiles(parameter, 8);
        for (auto &path : paths) {
            DEBUG_PRINT("OLDER: %s", path.c_str());
            SendFileResponse(datachannel, path);
        }
    } else if (type == protocol::QueryFileType::BEFORE_TIME) {
        auto path = Utils::FindFilesFromDatetime(args.record_path, parameter);
        DEBUG_PRINT("TIME_MATCH: %s", path.c_str());
        SendFileResponse(datachannel, path);
    }
}

void Conductor::SendFileResponse(std::shared_ptr<RtcChannel> datachannel, const std::string &path) {
    if (path.empty())
        return;

    protocol::QueryFileResponse resp;
    auto *file = resp.add_files();
    file->set_filepath(path);
    file->set_duration_sec(Utils::GetVideoDuration(path));

    auto last_dot = path.rfind('.');
    if (last_dot != std::string::npos) {
        std::string thumbnail_path = path.substr(0, last_dot) + ".jpg";

        auto binary_data = Utils::ReadFileInBinary(thumbnail_path);
        if (!binary_data.empty()) {
            file->set_thumbnail("data:image/jpeg;base64," + Utils::ToBase64(binary_data));
        }
    }

    datachannel->Send(resp);
}

void Conductor::TransferFile(std::shared_ptr<RtcChannel> datachannel, const protocol::Packet &pkt) {
    if (args.record_path.empty()) {
        return;
    }

    if (!pkt.has_transfer_file_request()) {
        ERROR_PRINT("Invalid file transfer request");
        return;
    }

    const std::string &path = pkt.transfer_file_request().filepath();

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

void Conductor::ControlCamera(std::shared_ptr<RtcChannel> datachannel,
                              const protocol::Packet &pkt) {
    if (!pkt.has_control_camera_request()) {
        ERROR_PRINT("Invalid camera control request");
        return;
    }

    const auto &req = pkt.control_camera_request();
    int key = req.id();
    int value = req.value();
    DEBUG_PRINT("parse meta cmd message => %d, %d", key, value);

    try {
        if (!args.use_libcamera) {
            throw std::runtime_error("Setting camera options only valid with libcamera.");
        }
        if (!video_capture_source_->SetControls(key, value)) {
            ERROR_PRINT("Failed to set key: %d to value: %d", key, value);
        }
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

    channel->RegisterHandler([this](const std::string &msg) {
        ipc_server_->Write(msg);
    });
    DEBUG_PRINT("DataChannel (%s) connected to IPC server for receiving.",
                channel->label().c_str());
}
