#include "rtc/rtc_channel.h"

#include "common/logging.h"

const int CHUNK_SIZE = 64 * 1024; // 64KB

std::shared_ptr<RtcChannel>
RtcChannel::Create(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
    return std::make_shared<RtcChannel>(std::move(data_channel));
}

RtcChannel::RtcChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel)
    : data_channel(data_channel),
      id_(Utils::GenerateUuid()),
      label_(data_channel->label()) {
    data_channel->RegisterObserver(this);
}
RtcChannel::~RtcChannel() { DEBUG_PRINT("datachannel (%s) is released!", label_.c_str()); }

std::string RtcChannel::id() const { return id_; }

std::string RtcChannel::label() const { return label_; }

void RtcChannel::OnStateChange() {
    webrtc::DataChannelInterface::DataState state = data_channel->state();
    DEBUG_PRINT("[%s] OnStateChange => %s", data_channel->label().c_str(),
                webrtc::DataChannelInterface::DataStateString(state));
}

void RtcChannel::Terminate() {
    data_channel->UnregisterObserver();
    data_channel->Close();
    if (on_closed_func_) {
        on_closed_func_();
    }
}

void RtcChannel::OnClosed(std::function<void()> func) { on_closed_func_ = std::move(func); }

void RtcChannel::OnMessage(const webrtc::DataBuffer &buffer) {
    const uint8_t *data = buffer.data.data<uint8_t>();
    size_t length = buffer.data.size();
    std::string message(reinterpret_cast<const char *>(data), length);

    Next(message);
}

void RtcChannel::RegisterHandler(protocol::CommandType type, CommandHandler func) {
    auto sub =
        observers_map_[type].Subscribe([self = shared_from_this(), func](protocol::Packet pkt) {
            func(self, pkt);
        });
    subscriptions_.push_back(std::move(sub));
}

void RtcChannel::RegisterHandler(CustomPayloadHandler func) {
    auto sub = custom_cmd_subject_.Subscribe([func](std::string message) {
        func(message);
    });
    subscriptions_.push_back(std::move(sub));
}

void RtcChannel::Next(const std::string &message) {
    protocol::Packet packet;
    if (!packet.ParseFromString(message)) {
        ERROR_PRINT("Failed to parse incoming packet");
        return;
    }

    DEBUG_PRINT("Received packet type: %s", protocol::CommandType_Name(packet.type()).c_str());

    if (packet.type() == protocol::CommandType::CUSTOM) {
        if (packet.has_custom_command()) {
            const std::string &payload = packet.custom_command();
            DEBUG_PRINT("CUSTOM payload: %s", payload.c_str());
            custom_cmd_subject_.Next(payload);
        } else {
            ERROR_PRINT("CUSTOM command without payload");
        }
        return;
    } else {
        observers_map_[packet.type()].Next(packet);
    }
}

void RtcChannel::SendStream(protocol::CommandType type, const uint8_t *data, size_t size) {
    auto stream_id = Utils::GenerateUuid();

    protocol::Packet header_pkt;
    header_pkt.set_type(type);
    auto *header = header_pkt.mutable_stream_header();
    header->set_stream_id(stream_id);
    header->set_total_length(size);

    std::string header_buf = header_pkt.SerializeAsString();
    Send((uint8_t *)header_buf.data(), header_buf.size());

    size_t offset = 0;
    while (offset < size) {
        protocol::Packet chunk_pkt;
        chunk_pkt.set_type(type);
        auto *chunk = chunk_pkt.mutable_stream_chunk();
        auto read_size = std::min((size_t)CHUNK_SIZE, size - offset);
        chunk->set_stream_id(stream_id);
        chunk->set_offset(offset);
        chunk->set_data(data + offset, read_size);

        std::string chunk_buf = chunk_pkt.SerializeAsString();
        Send((uint8_t *)chunk_buf.data(), chunk_buf.size());
        offset += read_size;
    }

    protocol::Packet trailer_pkt;
    trailer_pkt.set_type(type);
    auto *trailer = trailer_pkt.mutable_stream_trailer();
    trailer->set_stream_id(stream_id);

    std::string trailer_buf = trailer_pkt.SerializeAsString();
    Send((uint8_t *)trailer_buf.data(), trailer_buf.size());
}

void RtcChannel::Send(const uint8_t *data, size_t size) {
    if (data_channel->state() != webrtc::DataChannelInterface::kOpen) {
        return;
    }

    while (data_channel->buffered_amount() + size > data_channel->MaxSendQueueSize()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    rtc::CopyOnWriteBuffer buffer(data, size);
    webrtc::DataBuffer data_buffer(buffer, true);
    data_channel->Send(data_buffer);
}

void RtcChannel::Send(const protocol::QueryFileResponse &response) {
    std::string body;
    if (!response.SerializeToString(&body)) {
        ERROR_PRINT("Failed to serialize QueryFileResponse");
        return;
    }

    SendStream(protocol::CommandType::QUERY_FILE, (uint8_t *)body.c_str(), body.size());
}

void RtcChannel::Send(Buffer image) {
    SendStream(protocol::CommandType::TAKE_SNAPSHOT, (uint8_t *)image.start.get(), image.length);
    DEBUG_PRINT("Image sent: %lu bytes", image.length);
}

void RtcChannel::Send(std::ifstream &file) {
    if (!file.is_open())
        return;

    auto type = protocol::CommandType::TRANSFER_FILE;

    file.seekg(0, std::ios::end);
    size_t total_size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto stream_id = Utils::GenerateUuid();

    protocol::Packet header_pkt;
    header_pkt.set_type(type);
    auto *header = header_pkt.mutable_stream_header();
    header->set_stream_id(stream_id);
    header->set_total_length(total_size);

    std::string header_data = header_pkt.SerializeAsString();
    Send((uint8_t *)header_data.data(), header_data.size());

    std::vector<char> buffer(CHUNK_SIZE);
    std::vector<uint8_t> serialization_buf(CHUNK_SIZE + 256);
    size_t offset = 0;
    while (file.read(buffer.data(), CHUNK_SIZE) || file.gcount() > 0) {
        size_t read_size = file.gcount();

        protocol::Packet chunk_pkt;
        chunk_pkt.set_type(type);
        auto *chunk = chunk_pkt.mutable_stream_chunk();
        chunk->set_stream_id(stream_id);
        chunk->set_offset(offset);
        chunk->set_data(buffer.data(), read_size);

        std::string chunk_data = chunk_pkt.SerializeAsString();
        Send((uint8_t *)chunk_data.data(), chunk_data.size());

        offset += read_size;
    }

    protocol::Packet trailer_pkt;
    trailer_pkt.set_type(type);
    auto *trailer = trailer_pkt.mutable_stream_trailer();
    trailer->set_stream_id(stream_id);

    std::string trailer_data = trailer_pkt.SerializeAsString();
    Send((uint8_t *)trailer_data.data(), trailer_data.size());
}

void RtcChannel::Send(const std::string &message) {
    SendStream(protocol::CommandType::CUSTOM, (uint8_t *)message.c_str(), message.length());
}
