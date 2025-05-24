#include "rtc/sfu_channel.h"

#include "proto/livekit_models.pb.h"

#include "common/logging.h"

std::shared_ptr<SfuChannel>
SfuChannel::Create(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
    return std::make_shared<SfuChannel>(std::move(data_channel));
}

SfuChannel::SfuChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : RtcChannel(data_channel) {}

SfuChannel::~SfuChannel() {
    DEBUG_PRINT("sfu datachannel (%s) is released!", label().c_str());
}

void SfuChannel::OnMessage(const webrtc::DataBuffer &buffer) {
    const uint8_t *data = buffer.data.data<uint8_t>();
    size_t length = buffer.data.size();

    livekit::DataPacket packet;
    if (!packet.ParseFromArray(data, static_cast<int>(length))) {
        DEBUG_PRINT("(%s) Failed to parse DataPacket", label().c_str());
        return;
    }

    if (packet.has_user()) {
        const auto &user_data = packet.user();
        DEBUG_PRINT("(%s) Received USER packet: participant_sid=%s, payload=%s", label().c_str(),
                    user_data.participant_sid().c_str(), user_data.payload().c_str());
    } else if (packet.has_speaker()) {
        const auto &speaker_data = packet.speaker();
        DEBUG_PRINT("(%s) Received SPEAKER packet (not handled yet)", label().c_str());
    } else {
        DEBUG_PRINT("(%s) Unknown DataPacket type", label().c_str());
    }
}
