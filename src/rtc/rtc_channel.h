#ifndef DATA_CHANNEL_H_
#define DATA_CHANNEL_H_

#include <fstream>
#include <map>
#include <vector>

#include <api/data_channel_interface.h>
#include <nlohmann/json.hpp>

#include "common/interface/subject.h"
#include "common/utils.h"
#include "ipc/unix_socket_server.h"

using json = nlohmann::json;

enum class CommandType : uint8_t {
    CONNECT,
    SNAPSHOT,
    METADATA,
    RECORDING,
    CAMERA_OPTION,
    BROADCAST,
    CUSTOM
};

enum class MetadataCommand : uint8_t {
    LATEST,
    OLDER,
    SPECIFIC_TIME
};

struct RtcMessage {
    CommandType type;
    std::string message;
    RtcMessage(CommandType type, std::string message)
        : type(type),
          message(message) {}

    std::string ToString() const {
        json j;
        j["type"] = static_cast<int>(type);
        j["message"] = message;
        return j.dump();
    }
};

struct MetaMessage {
    std::string path;
    int duration;
    std::string image;

    MetaMessage(std::string file_path)
        : path(file_path),
          duration(Utils::GetVideoDuration(file_path)) {

        int dot_pos = file_path.rfind('.');
        auto thumbnail_path = file_path.substr(0, dot_pos) + ".jpg";
        auto binary_data = Utils::ReadFileInBinary(thumbnail_path);
        auto base64_data = Utils::ToBase64(binary_data);
        image = "data:image/jpeg;base64," + base64_data;
    }

    std::string ToString() const {
        json j;
        j["path"] = path;
        j["duration"] = duration;
        j["image"] = image;
        return j.dump();
    }
};

class RtcChannel : public webrtc::DataChannelObserver,
                   public Subject<std::string>,
                   public std::enable_shared_from_this<RtcChannel> {
  public:
    using ChannelCommandHandler =
        std::function<void(std::shared_ptr<RtcChannel>, const std::string &)>;
    using PayloadHandler = std::function<void(const std::string &)>;

    static std::shared_ptr<RtcChannel>
    Create(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

    RtcChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
    ~RtcChannel();

    std::string id() const;
    std::string label() const;

    // webrtc::DataChannelObserver
    void OnStateChange() override;
    void OnMessage(const webrtc::DataBuffer &buffer) override;
    void OnClosed(std::function<void()> func);

    void Terminate();
    void RegisterHandler(CommandType type, ChannelCommandHandler func);
    void RegisterHandler(CommandType type, PayloadHandler func);

    void Send(MetaMessage metadata);
    void Send(Buffer image);
    void Send(std::ifstream &file);
    void Send(const std::string &message);

  protected:
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;

    virtual void Send(const uint8_t *data, size_t size);
    void Next(std::string message) override final;

  private:
    std::string id_;
    std::string label_;
    std::function<void()> on_closed_func_;
    std::map<CommandType, std::vector<std::shared_ptr<Observable<std::string>>>> observers_map_;

    // Subject
    std::shared_ptr<Observable<std::string>> AsObservable() override;
    std::shared_ptr<Observable<std::string>> AsObservable(CommandType type);
    void UnSubscribe() override;

    void Send(CommandType type, const uint8_t *data, size_t size);
};

#endif // DATA_CHANNEL_H_
