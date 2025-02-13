#include "conductor.h"

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
    return ptr;
}

Conductor::Conductor(Args args)
    : args(args) {}

Args Conductor::config() const { return args; }

std::shared_ptr<PaCapturer> Conductor::AudioSource() const { return audio_capture_source_; }

std::shared_ptr<VideoCapturer> Conductor::VideoSource() const { return video_capture_source_; }

void Conductor::InitializeTracks() {
    if (audio_track_ == nullptr && !args.no_audio) {
        audio_capture_source_ = PaCapturer::Create(args);
        auto options = peer_connection_factory_->CreateAudioSource(cricket::AudioOptions());
        audio_track_ = peer_connection_factory_->CreateAudioTrack("audio_track", options.get());
    }

    if (video_track_ == nullptr && !args.device.empty()) {
        video_capture_source_ = ([this]() -> std::shared_ptr<VideoCapturer> {
            if (args.use_libcamera) {
                return LibcameraCapturer::Create(args);
            } else {
                return V4l2Capturer::Create(args);
            }
        })();

        video_track_source_ = ([this]() -> rtc::scoped_refptr<ScaleTrackSource> {
            if (args.hw_accel) {
                return V4l2DmaTrackSource::Create(video_capture_source_);
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

    std::string stream_id = "test_stream_id";

    if (audio_track_) {
        auto audio_res = peer_connection->AddTrack(audio_track_, {stream_id});
        if (!audio_res.ok()) {
            ERROR_PRINT("Failed to add audio track, %s", audio_res.error().message());
        }
    }

    if (video_track_) {
        auto video_res = peer_connection->AddTrack(video_track_, {stream_id});
        if (!video_res.ok()) {
            ERROR_PRINT("Failed to add video track, %s", video_res.error().message());
        }

        auto video_sender_ = video_res.value();
        webrtc::RtpParameters parameters = video_sender_->GetParameters();
        parameters.degradation_preference = webrtc::DegradationPreference::MAINTAIN_FRAMERATE;
        video_sender_->SetParameters(parameters);
    }
}

rtc::scoped_refptr<RtcPeer> Conductor::CreatePeerConnection(PeerConfig peer_config) {
    webrtc::PeerConnectionInterface::RTCConfiguration config;
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

    peer_config.timeout = args.peer_timeout;
    auto peer = RtcPeer::Create(std::move(peer_config));
    auto result = peer_connection_factory_->CreatePeerConnectionOrError(
        config, webrtc::PeerConnectionDependencies(peer.get()));

    if (!result.ok()) {
        DEBUG_PRINT("Peer connection is failed to create!");
        return nullptr;
    }

    peer->SetPeer(result.MoveValue());
    peer->CreateDataChannel();
    peer->OnSnapshot([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        OnSnapshot(datachannel, msg);
    });

    peer->OnMetadata([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        OnMetadata(datachannel, msg);
    });

    peer->OnRecord([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        OnRecord(datachannel, msg);
    });

    peer->OnCameraOption([this](std::shared_ptr<DataChannelSubject> datachannel, std::string msg) {
        OnCameraOption(datachannel, msg);
    });

    AddTracks(peer->GetPeer());

    DEBUG_PRINT("Peer connection(%s) is created! ", peer->GetId().c_str());
    return peer;
}

void Conductor::OnSnapshot(std::shared_ptr<DataChannelSubject> datachannel, std::string &msg) {
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

void Conductor::OnMetadata(std::shared_ptr<DataChannelSubject> datachannel, std::string &msg) {
    DEBUG_PRINT("OnMetadata msg: %s", msg.c_str());
    json jsonObj = json::parse(msg.c_str());

    MetadataCommand cmd = jsonObj["command"];
    std::string message = jsonObj["message"];
    DEBUG_PRINT("parse meta cmd message => %hhu, %s", cmd, message.c_str());

    if (args.record_path.empty()) {
        return;
    }
    try {
        if (cmd == MetadataCommand::LATEST) {
            auto latest_mp4_path = Utils::FindSecondNewestFile(args.record_path, ".mp4");
            MetaMessage metadata(latest_mp4_path);
            auto metadata_str = metadata.ToString();
            int file_size = metadata_str.length();
            std::string size_str = std::to_string(file_size);

            datachannel->Send(CommandType::METADATA, (uint8_t *)size_str.c_str(),
                              size_str.length());
            datachannel->Send(CommandType::METADATA, (uint8_t *)metadata_str.c_str(), file_size);
            datachannel->Send(CommandType::METADATA, nullptr, 0);
        } else if (cmd == MetadataCommand::OLDER) {
            if (message.empty()) {
                message = Utils::FindLatestFile(args.record_path, ".mp4");
            }
            auto paths = Utils::FindOlderFiles(message, 8);

            for (auto &path : paths) {
                MetaMessage metadata(path);
                auto metadata_str = metadata.ToString();
                int file_size = metadata_str.length();
                std::string size_str = std::to_string(file_size);

                datachannel->Send(CommandType::METADATA, (uint8_t *)size_str.c_str(),
                                  size_str.length());
                datachannel->Send(CommandType::METADATA, (uint8_t *)metadata_str.c_str(),
                                  file_size);
                datachannel->Send(CommandType::METADATA, nullptr, 0);
            }
        } else if (cmd == MetadataCommand::SPECIFIC_TIME) {
            auto path = Utils::FindFilesFromDatetime(args.record_path, message);

            MetaMessage metadata(path);
            auto metadata_str = metadata.ToString();
            int file_size = metadata_str.length();
            std::string size_str = std::to_string(file_size);

            datachannel->Send(CommandType::METADATA, (uint8_t *)size_str.c_str(),
                              size_str.length());
            datachannel->Send(CommandType::METADATA, (uint8_t *)metadata_str.c_str(), file_size);
            datachannel->Send(CommandType::METADATA, nullptr, 0);
        }
    } catch (const std::exception &e) {
        ERROR_PRINT("%s", e.what());
    }
}

void Conductor::OnRecord(std::shared_ptr<DataChannelSubject> datachannel, std::string &path) {
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

void Conductor::OnCameraOption(std::shared_ptr<DataChannelSubject> datachannel, std::string &msg) {
    DEBUG_PRINT("OnCameraControl msg: %s", msg.c_str());
    json jsonObj = json::parse(msg.c_str());

    int key = jsonObj["key"];
    int value = jsonObj["value"];
    DEBUG_PRINT("parse meta cmd message => %d, %d", key, value);

    try {
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

Conductor::~Conductor() {
    audio_track_ = nullptr;
    video_track_ = nullptr;
    video_capture_source_ = nullptr;
    peer_connection_factory_ = nullptr;
    rtc::CleanupSSL();
}
