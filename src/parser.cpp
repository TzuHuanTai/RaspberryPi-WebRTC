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
        ("fps", bpo::value<int>()->default_value(args.fps), "Set camera frame rate")
        ("width", bpo::value<int>()->default_value(args.width), "Set camera frame width")
        ("height", bpo::value<int>()->default_value(args.height), "Set camera frame height")
        ("jpeg_quality", bpo::value<int>()->default_value(args.jpeg_quality),
            "Set the default quality of the snapshot and thumbnail image")
        ("rotation_angle", bpo::value<int>()->default_value(args.rotation_angle),
            "Set the rotation angle of the frame")
        ("peer_timeout", bpo::value<int>()->default_value(args.peer_timeout),
            "The connection timeout, in seconds, after receiving a remote offer")
        ("segment_duration", bpo::value<int>()->default_value(args.segment_duration),
            "The length (in seconds) of each MP4 recording.")
        ("camera", bpo::value<std::string>()->default_value(args.camera),
            "Specify the camera using V4L2 or Libcamera. "
            "Examples: \"libcamera:0\" for Libcamera, \"v4l2:0\" for V4L2 at `/dev/video0`.")
        ("fixed_resolution", bpo::bool_switch()->default_value(args.fixed_resolution),
            "Disable adaptive resolution scaling and keep a fixed resolution.")
        ("no_audio", bpo::bool_switch()->default_value(args.no_audio), "Run without audio source")
        ("uid", bpo::value<std::string>()->default_value(args.uid),
            "Set the unique id to identify the device")
        ("stun_url", bpo::value<std::string>()->default_value(args.stun_url),
            "Stun server, ex: stun:xxx.xxx.xxx")(
        "turn_url", bpo::value<std::string>()->default_value(args.turn_url),
            "Turn server, ex: turn:xxx.xxx.xxx:3478?transport=tcp")
        ("turn_username", bpo::value<std::string>()->default_value(args.turn_username),
            "Turn server username")
        ("turn_password", bpo::value<std::string>()->default_value(args.turn_password),
            "Turn server password")
        ("mqtt_port", bpo::value<int>()->default_value(args.mqtt_port), "Mqtt server port")
        ("mqtt_host", bpo::value<std::string>()->default_value(args.mqtt_host),
            "Mqtt server host")
        ("mqtt_username", bpo::value<std::string>()->default_value(args.mqtt_username),
            "Mqtt server username")
        ("mqtt_password", bpo::value<std::string>()->default_value(args.mqtt_password),
            "Mqtt server password")
        ("http_port", bpo::value<uint16_t>()->default_value(args.http_port), "Http server port")
        ("ws_host", bpo::value<std::string>()->default_value(args.ws_host),
            "Websocket server host")
        ("ws_room", bpo::value<std::string>()->default_value(args.ws_room),
            "The room name to join")
        ("ws_key", bpo::value<std::string>()->default_value(args.ws_key),
            "Websocket server api key")
        ("record_path", bpo::value<std::string>()->default_value(args.record_path),
            "The path to save the recording video files. The recorder won't start if it's empty")
        ("hw_accel", bpo::bool_switch()->default_value(args.hw_accel),
            "Share DMA buffers between decoder/scaler/encoder, which can decrease cpu usage")
        ("use_mqtt", bpo::bool_switch()->default_value(args.use_mqtt),
            "Use mqtt to exchange sdp and ice candidates")
        ("use_whep", bpo::bool_switch()->default_value(args.use_whep),
            "Use whep to exchange sdp and ice candidates")
        ("use_websocket", bpo::bool_switch()->default_value(args.use_websocket),
            "Use websocket to exchange sdp and ice candidates")
        ("use_tls", bpo::bool_switch()->default_value(args.use_tls),
            "Use TLS for websocket connection")
        ("v4l2_format", bpo::value<std::string>()->default_value(args.v4l2_format),
            "Set v4l2 camera capture format to `i420`, `mjpeg`, `h264`. The `h264` can pass "
            "packets into mp4 without encoding to reduce cpu usage."
            "Use `v4l2-ctl -d /dev/videoX --list-formats` can list available format");
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

    SetIfExists(vm, "fps", args.fps);
    SetIfExists(vm, "width", args.width);
    SetIfExists(vm, "height", args.height);
    SetIfExists(vm, "jpeg_quality", args.jpeg_quality);
    SetIfExists(vm, "rotation_angle", args.rotation_angle);
    SetIfExists(vm, "peer_timeout", args.peer_timeout);
    SetIfExists(vm, "segment_duration", args.segment_duration);
    SetIfExists(vm, "camera", args.camera);
    SetIfExists(vm, "v4l2_format", args.v4l2_format);
    SetIfExists(vm, "uid", args.uid);
    SetIfExists(vm, "stun_url", args.stun_url);
    SetIfExists(vm, "turn_url", args.turn_url);
    SetIfExists(vm, "turn_username", args.turn_username);
    SetIfExists(vm, "turn_password", args.turn_password);
    SetIfExists(vm, "mqtt_port", args.mqtt_port);
    SetIfExists(vm, "mqtt_host", args.mqtt_host);
    SetIfExists(vm, "mqtt_username", args.mqtt_username);
    SetIfExists(vm, "mqtt_password", args.mqtt_password);
    SetIfExists(vm, "http_port", args.http_port);
    SetIfExists(vm, "ws_host", args.ws_host);
    SetIfExists(vm, "ws_room", args.ws_room);
    SetIfExists(vm, "ws_key", args.ws_key);
    SetIfExists(vm, "record_path", args.record_path);

    args.fixed_resolution = vm["fixed_resolution"].as<bool>();
    args.no_audio = vm["no_audio"].as<bool>();
    args.hw_accel = vm["hw_accel"].as<bool>();
    args.use_mqtt = vm["use_mqtt"].as<bool>();
    args.use_whep = vm["use_whep"].as<bool>();
    args.use_websocket = vm["use_websocket"].as<bool>();
    args.use_tls = vm["use_tls"].as<bool>();

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
