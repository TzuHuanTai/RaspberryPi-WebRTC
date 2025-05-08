#include "parser.h"

#include <boost/program_options.hpp>
#include <iostream>
#include <string>

namespace bpo = boost::program_options;

static const std::unordered_map<std::string, int> format_map = {
    {"mjpeg", V4L2_PIX_FMT_MJPEG},
    {"h264", V4L2_PIX_FMT_H264},
    {"i420", V4L2_PIX_FMT_YUV420},
};

template <typename T> void SetIfExists(bpo::variables_map &vm, const std::string &key, T &arg) {
    if (vm.count(key)) {
        arg = vm[key].as<T>();
    }
}

void Parser::ParseArgs(int argc, char *argv[], Args &args) {
    bpo::options_description opts("Options");

    // clang-format off
    opts.add_options()
        ("help,h", "Display the help message")
        ("camera", bpo::value<std::string>()->default_value(args.camera),
            "Specify the camera using V4L2 or Libcamera. "
            "e.g. \"libcamera:0\" for Libcamera, \"v4l2:0\" for V4L2 at `/dev/video0`.")
        ("v4l2-format", bpo::value<std::string>()->default_value(args.v4l2_format),
            "The input format (`i420`, `mjpeg`, `h264`) of the V4L2 camera.")
        ("uid", bpo::value<std::string>()->default_value(args.uid),
            "The unique id to identify the device.")
        ("fps", bpo::value<int>()->default_value(args.fps), "Specify the camera frames per second.")
        ("width", bpo::value<int>()->default_value(args.width), "Set camera frame width.")
        ("height", bpo::value<int>()->default_value(args.height), "Set camera frame height.")
        ("rotation-angle", bpo::value<int>()->default_value(args.rotation_angle),
            "Set the rotation angle of the camera (0, 90, 180, 270).")
        ("sample-rate", bpo::value<int>()->default_value(args.sample_rate),
            "Set the audio sample rate (in Hz).")
        ("no-audio", bpo::bool_switch()->default_value(args.no_audio), "Runs without audio source.")
        ("record-path", bpo::value<std::string>()->default_value(args.record_path),
            "Set the path where recording video files will be saved. "
            "If the value is empty or unavailable, the recorder will not start.")
        ("file-duration", bpo::value<int>()->default_value(args.file_duration),
            "The length (in seconds) of each MP4 recording.")
        ("jpeg-quality", bpo::value<int>()->default_value(args.jpeg_quality),
            "Set the quality of the snapshot and thumbnail images in range 0 to 100.")
        ("peer-timeout", bpo::value<int>()->default_value(args.peer_timeout),
            "The connection timeout (in seconds) after receiving a remote offer")
        ("hw-accel", bpo::bool_switch()->default_value(args.hw_accel),
            "Enable hardware acceleration by sharing DMA buffers between the decoder, "
            "scaler, and encoder to reduce CPU usage.")
        ("no-adaptive", bpo::bool_switch()->default_value(args.no_adaptive),
            "Disable WebRTC's adaptive resolution scaling. When enabled, "
            "the output resolution will remain fixed regardless of network or device conditions.")
        ("stun-url", bpo::value<std::string>()->default_value(args.stun_url),
            "Set the STUN server URL for WebRTC. e.g. `stun:xxx.xxx.xxx`.")
        ("turn-url", bpo::value<std::string>()->default_value(args.turn_url),
            "Set the TURN server URL for WebRTC. e.g. `turn:xxx.xxx.xxx:3478?transport=tcp`.")
        ("turn-username", bpo::value<std::string>()->default_value(args.turn_username),
            "Set the TURN server username for WebRTC authentication.")
        ("turn-password", bpo::value<std::string>()->default_value(args.turn_password),
            "Set the TURN server password for WebRTC authentication.")
        ("use-mqtt", bpo::bool_switch()->default_value(args.use_mqtt),
            "Use MQTT to exchange sdp and ice candidates.")
        ("mqtt-host", bpo::value<std::string>()->default_value(args.mqtt_host),
            "Set the MQTT server host.")
        ("mqtt-port", bpo::value<int>()->default_value(args.mqtt_port), "Set the MQTT server port.")
        ("mqtt-username", bpo::value<std::string>()->default_value(args.mqtt_username),
            "Set the MQTT server username.")
        ("mqtt-password", bpo::value<std::string>()->default_value(args.mqtt_password),
            "Set the MQTT server password.")
        ("use-whep", bpo::bool_switch()->default_value(args.use_whep),
            "Use WHEP (WebRTC-HTTP Egress Protocol) to exchange SDP and ICE candidates.")
        ("http-port", bpo::value<uint16_t>()->default_value(args.http_port),
            "Local HTTP server port to handle signaling when using WHEP.")
        ("use-websocket", bpo::bool_switch()->default_value(args.use_websocket),
            "Enables the WebSocket client to connect to the SFU server.")
        ("use-tls", bpo::bool_switch()->default_value(args.use_tls),
            "Use TLS for the WebSocket connection. Use it when connecting to a `wss://` URL.")
        ("ws-host", bpo::value<std::string>()->default_value(args.ws_host),
            "The WebSocket host address of the SFU server.")
        ("ws-room", bpo::value<std::string>()->default_value(args.ws_room),
            "The room name to join on the SFU server.")
        ("ws-key", bpo::value<std::string>()->default_value(args.ws_key),
            "The API key used to authenticate with the SFU server.");
    // clang-format on

    bpo::variables_map vm;
    try {
        bpo::store(bpo::parse_command_line(argc, argv, opts), vm);
        bpo::notify(vm);
    } catch (const bpo::error &ex) {
        std::cerr << "Error parsing arguments: " << ex.what() << std::endl;
        exit(1);
    }

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        exit(1);
    }

    SetIfExists(vm, "camera", args.camera);
    SetIfExists(vm, "v4l2-format", args.v4l2_format);
    SetIfExists(vm, "uid", args.uid);
    SetIfExists(vm, "fps", args.fps);
    SetIfExists(vm, "width", args.width);
    SetIfExists(vm, "height", args.height);
    SetIfExists(vm, "rotation-angle", args.rotation_angle);
    SetIfExists(vm, "sample-rate", args.sample_rate);
    SetIfExists(vm, "record-path", args.record_path);
    SetIfExists(vm, "file-duration", args.file_duration);
    SetIfExists(vm, "jpeg-quality", args.jpeg_quality);
    SetIfExists(vm, "peer-timeout", args.peer_timeout);
    SetIfExists(vm, "stun-url", args.stun_url);
    SetIfExists(vm, "turn-url", args.turn_url);
    SetIfExists(vm, "turn-username", args.turn_username);
    SetIfExists(vm, "turn-password", args.turn_password);
    SetIfExists(vm, "mqtt-host", args.mqtt_host);
    SetIfExists(vm, "mqtt-port", args.mqtt_port);
    SetIfExists(vm, "mqtt-username", args.mqtt_username);
    SetIfExists(vm, "mqtt-password", args.mqtt_password);
    SetIfExists(vm, "http-port", args.http_port);
    SetIfExists(vm, "ws-host", args.ws_host);
    SetIfExists(vm, "ws-room", args.ws_room);
    SetIfExists(vm, "ws-key", args.ws_key);

    args.no_audio = vm["no-audio"].as<bool>();
    args.hw_accel = vm["hw-accel"].as<bool>();
    args.no_adaptive = vm["no-adaptive"].as<bool>();
    args.use_mqtt = vm["use-mqtt"].as<bool>();
    args.use_whep = vm["use-whep"].as<bool>();
    args.use_websocket = vm["use-websocket"].as<bool>();
    args.use_tls = vm["use-tls"].as<bool>();

    if (!args.stun_url.empty() && args.stun_url.substr(0, 4) != "stun") {
        std::cout << "Stun url should not be empty and start with \"stun:\"" << std::endl;
        exit(1);
    }

    if (!args.turn_url.empty() && args.turn_url.substr(0, 4) != "turn") {
        std::cout << "Turn url should start with \"turn:\"" << std::endl;
        exit(1);
    }

    if (!args.record_path.empty()) {
        if (args.record_path.front() != '/') {
            std::cout << "The file path needs to start with a \"/\" character" << std::endl;
            exit(1);
        }
        if (args.record_path.back() != '/') {
            args.record_path += '/';
        }
    }

    ParseDevice(args);
}

void Parser::ParseDevice(Args &args) {
    size_t pos = args.camera.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("Unknown device format: " + args.camera);
    }

    std::string prefix = args.camera.substr(0, pos);
    std::string id = args.camera.substr(pos + 1);

    try {
        args.cameraId = std::stoi(id);
    } catch (const std::exception &e) {
        throw std::runtime_error("Invalid camera ID: " + id);
    }

    if (prefix == "libcamera") {
        args.use_libcamera = true;
        args.format = V4L2_PIX_FMT_YUV420;
        std::cout << "Using Libcamera, ID: " << args.cameraId << std::endl;
    } else if (prefix == "v4l2") {
        auto it = format_map.find(args.v4l2_format);
        if (it != format_map.end()) {
            args.format = it->second;
            std::cout << "Using V4L2: " << args.v4l2_format << std::endl;
        } else {
            throw std::runtime_error("Unsupported format: " + args.v4l2_format);
        }
        std::cout << "Using V4L2, ID: " << args.cameraId << std::endl;
    } else {
        throw std::runtime_error("Unknown device format: " + prefix);
    }
}
