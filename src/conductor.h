#ifndef CONDUCTOR_H_
#define CONDUCTOR_H_

#include "args.h"
#include "rtc_peer.h"
#include "capture/pa_capture.h"
#include "capture/v4l2_capture.h"
#include "track/swscale_track_source.h"

#include <iostream>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <condition_variable>

#include <rtc_base/thread.h>
#include <api/peer_connection_interface.h>

class Conductor {
public:
    static std::shared_ptr<Conductor> Create(Args args);

    Conductor(Args args);
    ~Conductor();

    Args config() const;
    bool CreatePeerConnection();
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> GetPeer() const;
    std::shared_ptr<PaCapture> AudioSource() const;
    std::shared_ptr<V4L2Capture> VideoSource() const;
    void SetSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *video_sink_obj);
    bool IsReady() const;
    void AwaitCompletion();

private:
    Args args;
    int peers_idx;
    bool is_ready;
    std::mutex state_mtx;
    std::condition_variable ready_state;
    std::unordered_map<int, rtc::scoped_refptr<RtcPeer>> peers_map;

    void InitializePeerConnectionFactory();
    void InitializeTracks();
    bool InitializeRecorder();
    void AddTracks(rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection);
    void RefreshPeerList();
    void SetPeerReadyState(bool state);

    std::unique_ptr<rtc::Thread> network_thread_;
    std::unique_ptr<rtc::Thread> worker_thread_;
    std::unique_ptr<rtc::Thread> signaling_thread_;
    rtc::scoped_refptr<RtcPeer> peer_;
    std::shared_ptr<PaCapture> audio_capture_source_;
    std::shared_ptr<V4L2Capture> video_capture_source_;
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
    rtc::scoped_refptr<SwScaleTrackSource> video_track_source_;
};

#endif // CONDUCTOR_H_
