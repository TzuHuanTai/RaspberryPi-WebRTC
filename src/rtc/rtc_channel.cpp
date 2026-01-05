#include "rtc/rtc_channel.h"

#include "common/logging.h"

const int CHUNK_SIZE = 65536;

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

void RtcChannel::RegisterHandler(CommandType type, ChannelCommandHandler func) {
    auto sub =
        observers_map_[type].Subscribe([self = shared_from_this(), func](std::string message) {
            if (!message.empty()) {
                func(self, message);
            }
        });
    subscriptions_.push_back(std::move(sub));
}

void RtcChannel::RegisterHandler(CommandType type, PayloadHandler func) {
    auto sub = observers_map_[type].Subscribe([func](std::string message) {
        if (!message.empty()) {
            func(message);
        }
    });
    subscriptions_.push_back(std::move(sub));
}

void RtcChannel::Next(const std::string &message) {
    try {
        json jsonObj = json::parse(message.c_str());

        std::string jsonStr = jsonObj.dump();
        DEBUG_PRINT("Receive message => %s", jsonStr.c_str());

        CommandType type = jsonObj["type"];
        std::string content = jsonObj["message"];
        if (content.empty()) {
            return;
        }

        observers_map_[type].Next(content);

    } catch (const json::parse_error &e) {
        ERROR_PRINT("JSON parse error, %s, occur at position: %lu", e.what(), e.byte);
    }
}

void RtcChannel::Send(CommandType type, const uint8_t *data, size_t size) {
    int bytes_read = 0;
    const size_t header_size = sizeof(CommandType);

    std::vector<char> data_with_header(CHUNK_SIZE);

    if (size == 0) {
        std::memcpy(data_with_header.data(), &type, header_size);
        Send((uint8_t *)data_with_header.data(), header_size);
        return;
    }

    while (bytes_read < size) {
        if (data_channel->buffered_amount() + CHUNK_SIZE > data_channel->MaxSendQueueSize()) {
            usleep(100);
            DEBUG_PRINT("Sleeping for 100 microsecond due to MaxSendQueueSize reached.");
            continue;
        }
        int read_size = std::min(CHUNK_SIZE - header_size, size - bytes_read);

        std::memcpy(data_with_header.data(), &type, header_size);
        std::memcpy(data_with_header.data() + header_size, data + bytes_read, read_size);

        Send((uint8_t *)data_with_header.data(), read_size + header_size);
        bytes_read += read_size;
    }
}

void RtcChannel::Send(const uint8_t *data, size_t size) {
    if (data_channel->state() != webrtc::DataChannelInterface::kOpen) {
        return;
    }

    rtc::CopyOnWriteBuffer buffer(data, size);
    webrtc::DataBuffer data_buffer(buffer, true);
    data_channel->Send(data_buffer);
}

void RtcChannel::Send(MetaMessage metadata) {
    if (metadata.path.empty()) {
        return;
    }

    auto type = CommandType::METADATA;
    auto body = metadata.ToString();
    int body_size = body.length();
    auto header = std::to_string(body_size);
    int header_size = header.length();

    Send(type, (uint8_t *)header.c_str(), header_size);
    Send(type, (uint8_t *)body.c_str(), body_size);
    Send(type, nullptr, 0);
}

void RtcChannel::Send(Buffer image) {
    auto type = CommandType::SNAPSHOT;
    const int file_size = image.length;
    std::string size_str = std::to_string(file_size);
    Send(type, (uint8_t *)size_str.c_str(), size_str.length());
    Send(type, (uint8_t *)image.start.get(), file_size);
    Send(type, nullptr, 0);

    DEBUG_PRINT("Image sent: %d bytes", file_size);
}

void RtcChannel::Send(std::ifstream &file) {
    std::vector<char> buffer(CHUNK_SIZE);
    int bytes_read = 0;
    int count = 0;
    const int header_size = sizeof(CommandType);
    auto type = CommandType::RECORDING;

    int file_size = file.tellg();
    std::string size_str = std::to_string(file_size);
    file.seekg(0, std::ios::beg);

    Send(type, (uint8_t *)size_str.c_str(), size_str.length());

    while (bytes_read < file_size) {
        if (data_channel->buffered_amount() + CHUNK_SIZE > data_channel->MaxSendQueueSize()) {
            sleep(1);
            DEBUG_PRINT("Sleeping for 1 second due to MaxSendQueueSize reached.");
            continue;
        }
        int read_size = std::min(CHUNK_SIZE - header_size, file_size - bytes_read);
        std::memcpy(buffer.data(), &type, header_size);
        file.read(buffer.data() + header_size, read_size);

        Send((uint8_t *)buffer.data(), read_size + header_size);
        bytes_read += read_size;
    }

    Send(type, nullptr, 0);
}

void RtcChannel::Send(const std::string &message) {
    auto type = CommandType::CUSTOM;
    auto body = message;
    int body_size = message.length();
    auto header = std::to_string(body_size);
    int header_size = header.length();

    Send(type, (uint8_t *)header.c_str(), header_size);
    Send(type, (uint8_t *)body.c_str(), body_size);
    Send(type, nullptr, 0);
}
