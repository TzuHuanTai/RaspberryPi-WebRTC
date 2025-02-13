#ifndef CONDUCTOR_H_
#define CONDUCTOR_H_

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <api/peer_connection_interface.h>
#include <rtc_base/thread.h>

#include "args.h"
#include "capturer/pa_capturer.h"
#include "capturer/video_capturer.h"
#include "rtc_peer.h"
#include "track/scale_track_source.h"

class Conductor {
  public:
    static std::shared_ptr<Conductor> Create(Args args);

    Conductor(Args args);
    ~Conductor();

    Args config() const;
    rtc::scoped_refptr<RtcPeer> CreatePeerConnection(PeerConfig peer_config);
    std::shared_ptr<PaCapturer> AudioSource() const;
    std::shared_ptr<VideoCapturer> VideoSource() const;

  private:
    Args args;

    void InitializePeerConnectionFactory();
    void InitializeTracks();
    void AddTracks(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);
    void OnSnapshot(std::shared_ptr<DataChannelSubject> datachannel, std::string &msg);
    void OnMetadata(std::shared_ptr<DataChannelSubject> datachannel, std::string &path);
    void OnRecord(std::shared_ptr<DataChannelSubject> datachannel, std::string &path);
    void OnCameraOption(std::shared_ptr<DataChannelSubject> datachannel, std::string &msg);

    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;

    std::shared_ptr<PaCapturer> audio_capture_source_;
    std::shared_ptr<VideoCapturer> video_capture_source_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<ScaleTrackSource> video_track_source_;
};

#endif // CONDUCTOR_H_
