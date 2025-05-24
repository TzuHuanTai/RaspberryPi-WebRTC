#include "rtc/sfu_channel.h"

#include "proto/livekit_models.pb.h"

#include "common/logging.h"

std::shared_ptr<SfuChannel>
SfuChannel::Create(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
    return std::make_shared<SfuChannel>(std::move(data_channel));
}

SfuChannel::SfuChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : RtcChannel(data_channel),
      topic_("ipc_topic") {}

SfuChannel::~SfuChannel() { DEBUG_PRINT("sfu datachannel (%s) is released!", label().c_str()); }

void SfuChannel::OnMessage(const webrtc::DataBuffer &buffer) {
    livekit::DataPacket packet;
    if (!packet.ParseFromArray(buffer.data.data(), buffer.data.size())) {
        DEBUG_PRINT("(%s) Failed to parse DataPacket", label().c_str());
        return;
    }

    if (!packet.has_user()) {
        DEBUG_PRINT("(%s) Unknown DataPacket type", label().c_str());
        return;
    }

    const auto &user = packet.user();
    std::string topic = user.topic();
    const std::string &payload = user.payload();
    DEBUG_PRINT("(%s) Received USER packet: participant_sid=%s, payload=%s, topic=%s",
                label().c_str(), user.participant_sid().c_str(), payload.c_str(), topic.c_str());

    Next(payload);
}

void SfuChannel::Send(const uint8_t *data, size_t size) { SendUserData(topic_, data, size); }

void SfuChannel::SendUserData(const std::string &topic, const uint8_t *data, size_t size) {
    if (data_channel->state() != webrtc::DataChannelInterface::kOpen) {
        return;
    }

    livekit::UserPacket user;
    user.set_payload(data, size);
    user.set_topic(topic);

    livekit::DataPacket packet;
    packet.set_allocated_user(&user);

    std::string serialized;
    if (!packet.SerializeToString(&serialized)) {
        return;
    }

    rtc::CopyOnWriteBuffer buffer(serialized.data(), serialized.size());
    webrtc::DataBuffer data_buffer(buffer, true);
    data_channel->Send(data_buffer);

    (void)packet.release_user();
}
