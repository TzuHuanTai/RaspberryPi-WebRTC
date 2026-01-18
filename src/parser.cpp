#include "parser.h"
#include "recorder/recorder_manager.h"
#include "rtc/rtc_peer.h"

#include <algorithm>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>

#if defined(USE_LIBCAMERA_CAPTURE)
#include <libcamera/libcamera.h>
#endif

namespace bpo = boost::program_options;

static const std::unordered_map<std::string, int> v4l2_fmt_table = {
    {"mjpeg", V4L2_PIX_FMT_MJPEG},
    {"h264", V4L2_PIX_FMT_H264},
    {"i420", V4L2_PIX_FMT_YUV420},
    {"yuyv", V4L2_PIX_FMT_YUYV},
};

static const std::unordered_map<std::string, int> record_mode_table = {
    {"both", -1},
    {"video", RecordMode::Video},
    {"snapshot", RecordMode::Snapshot},
};

static const std::unordered_map<std::string, int> ipc_mode_table = {
    {"both", -1},
    {"lossy", ChannelMode::Lossy},
    {"reliable", ChannelMode::Reliable},
};

#if defined(USE_LIBCAMERA_CAPTURE)
static const std::unordered_map<std::string, int> ae_metering_table = {
    {"centre", libcamera::controls::MeteringCentreWeighted},
    {"spot", libcamera::controls::MeteringSpot},
    {"average", libcamera::controls::MeteringMatrix},
    {"matrix", libcamera::controls::MeteringMatrix},
    {"custom", libcamera::controls::MeteringCustom}};

static const std::unordered_map<std::string, int> exposure_table = {
    {"normal", libcamera::controls::ExposureNormal},
    {"sport", libcamera::controls::ExposureShort},
    {"short", libcamera::controls::ExposureShort},
    {"long", libcamera::controls::ExposureLong},
    {"custom", libcamera::controls::ExposureCustom}};

static const std::unordered_map<std::string, int> awb_table = {
    {"auto", libcamera::controls::AwbAuto},
    {"normal", libcamera::controls::AwbAuto},
    {"incandescent", libcamera::controls::AwbIncandescent},
    {"tungsten", libcamera::controls::AwbTungsten},
    {"fluorescent", libcamera::controls::AwbFluorescent},
    {"indoor", libcamera::controls::AwbIndoor},
    {"daylight", libcamera::controls::AwbDaylight},
    {"cloudy", libcamera::controls::AwbCloudy},
    {"custom", libcamera::controls::AwbCustom}};

static const std::unordered_map<std::string, int> denoise_table = {
    {"auto", libcamera::controls::draft::NoiseReductionModeFast},
    {"off", libcamera::controls::draft::NoiseReductionModeOff},
    {"cdn_off", libcamera::controls::draft::NoiseReductionModeMinimal},
    {"cdn_fast", libcamera::controls::draft::NoiseReductionModeFast},
    {"cdn_hq", libcamera::controls::draft::NoiseReductionModeHighQuality}};

static const std::unordered_map<std::string, int> afMode_table = {
    {"default", -1},
    {"manual", libcamera::controls::AfModeEnum::AfModeManual},
    {"auto", libcamera::controls::AfModeEnum::AfModeAuto},
    {"continuous", libcamera::controls::AfModeEnum::AfModeContinuous}};

static const std::unordered_map<std::string, int> afRange_table = {
    {"normal", libcamera::controls::AfRangeNormal},
    {"macro", libcamera::controls::AfRangeMacro},
    {"full", libcamera::controls::AfRangeFull}};

static const std::unordered_map<std::string, int> afSpeed_table = {
    {"normal", libcamera::controls::AfSpeedNormal}, {"fast", libcamera::controls::AfSpeedFast}};
#endif

inline int ParseEnum(const std::unordered_map<std::string, int> table, const std::string &str) {
    auto it = table.find(str);
    if (it == table.end()) {
        throw std::invalid_argument("Invalid enum string: " + str);
    }
    return it->second;
}

void Parser::ParseArgs(int argc, char *argv[], Args &args) {
    bpo::options_description opts("Options");

    // clang-format off
    opts.add_options()
        ("help,h", "Display the help message")
        ("camera", bpo::value<std::string>(&args.camera)->default_value(args.camera),
            "Specify the camera using V4L2 or Libcamera. "
            "e.g. \"libcamera:0\" for Libcamera, \"v4l2:0\" for V4L2 at `/dev/video0`.")
        ("v4l2-format", bpo::value<std::string>(&args.v4l2_format)->default_value(args.v4l2_format),
            "The input format (`i420`, `yuyv`, `mjpeg`, `h264`) of the V4L2 camera.")
        ("uid", bpo::value<std::string>(&args.uid)->default_value(args.uid),
            "The unique id to identify the device.")
        ("fps", bpo::value<int>(&args.fps)->default_value(args.fps), "Specify the camera frames per second.")
        ("width", bpo::value<int>(&args.width)->default_value(args.width), "Set camera frame width.")
        ("height", bpo::value<int>(&args.height)->default_value(args.height), "Set camera frame height.")
        ("rotation", bpo::value<int>(&args.rotation)->default_value(args.rotation),
            "Set the rotation angle of the camera (0, 90, 180, 270).")
        ("sub-width", bpo::value<int>(&args.sub_width)->default_value(args.sub_width),
            "Set sub stream frame width for AI processing, default is 0 (disabled).")
        ("sub-height", bpo::value<int>(&args.sub_height)->default_value(args.sub_height),
            "Set sub stream frame height for AI processing, default is 0 (disabled).")
        ("record-stream", bpo::value<int>(&args.record_stream_idx)->default_value(args.record_stream_idx),
            "Recording stream index, 0: main stream, 1: sub stream")
        ("live-stream", bpo::value<int>(&args.live_stream_idx)->default_value(args.live_stream_idx),
            "Live stream index, 0: main stream, 1: sub stream")
        ("ai-stream", bpo::value<int>(&args.ai_stream_idx)->default_value(args.ai_stream_idx),
            "AI stream index, 0: main stream, 1: sub stream")
        ("sample-rate", bpo::value<int>(&args.sample_rate)->default_value(args.sample_rate),
            "Set the audio sample rate (in Hz).")
        ("no-audio", bpo::bool_switch(&args.no_audio)->default_value(args.no_audio), "Runs without audio source.")
#if defined(USE_LIBCAMERA_CAPTURE)
        ("sharpness", bpo::value<float>(&args.sharpness)->default_value(args.sharpness),
            "Adjust the sharpness of the libcamera output in range 0.0 to 15.99")
        ("contrast", bpo::value<float>(&args.contrast)->default_value(args.contrast),
            "Adjust the contrast of the libcamera output in range 0.0 to 15.99")
        ("brightness", bpo::value<float>(&args.brightness)->default_value(args.brightness),
            "Adjust the brightness of the libcamera output in range -1.0 to 1.0")
        ("saturation", bpo::value<float>(&args.saturation)->default_value(args.saturation),
            "Adjust the saturation of the libcamera output in range 0.0 to 15.99")
        ("ev", bpo::value<float>(&args.ev)->default_value(args.ev),
            "Set the EV (exposure value compensation) in range -10.0 to 10.0")
        ("shutter", bpo::value<std::string>(&args.shutter_)->default_value(args.shutter_),
            "Set manual shutter speed in microseconds (0 = auto)")
        ("gain", bpo::value<float>(&args.gain)->default_value(args.gain),
            "Set manual analog gain (0 = auto)")
        ("metering", bpo::value<std::string>(&args.ae_metering)->default_value(args.ae_metering),
            "Metering mode: centre, spot, average, custom")
        ("exposure", bpo::value<std::string>(&args.exposure)->default_value(args.exposure),
            "Exposure mode: normal, sport, short, long, custom")
        ("awb", bpo::value<std::string>(&args.awb)->default_value(args.awb),
            "Awb mode: auto, incandescent, tungsten, fluorescent, indoor, daylight, cloudy, custom")
        ("awbgains", bpo::value<std::string>(&args.awbgains)->default_value(args.awbgains),
            "Custom AWB gains as comma-separated Red, Blue values. e.g. '1.2,1.5'")
        ("denoise", bpo::value<std::string>(&args.denoise)->default_value(args.denoise),
            "Denoise mode: off, cdn_off, cdn_fast, cdn_hq, auto")
        ("tuning-file", bpo::value<std::string>(&args.tuning_file)->default_value(args.tuning_file),
			"Name of camera tuning file to use, omit this option for libcamera default behaviour")
        ("autofocus-mode", bpo::value<std::string>(&args.autofocus_mode)->default_value(args.autofocus_mode),
            "Autofocus mode: default, manual, auto, continuous")
        ("autofocus-range", bpo::value<std::string>(&args.af_range)->default_value(args.af_range),
            "Autofocus range: normal, macro, full")
        ("autofocus-speed", bpo::value<std::string>(&args.af_speed)->default_value(args.af_speed),
            "Autofocus speed: normal, fast")
        ("autofocus-window", bpo::value<std::string>(&args.af_window)->default_value(args.af_window),
            "Autofocus window as x,y,width,height. e.g. '0.3,0.3,0.4,0.4'")
        ("lens-position", bpo::value<std::string>(&args.lens_position_)->default_value(args.lens_position_),
            "Set the lens to a particular focus position, \"0\" moves the lens to infinity, or \"default\" for the hyperfocal distance")
#endif
        ("record-mode", bpo::value<std::string>(&args.record)->default_value(args.record),
            "Recording mode: 'video' to record MP4 files, 'snapshot' to save periodic JPEG images, "
            "or 'both' to do both simultaneously.")
        ("record-path", bpo::value<std::string>(&args.record_path)->default_value(args.record_path),
            "Set the path where recording video files will be saved. "
            "If the value is empty or unavailable, the recorder will not start.")
        ("file-duration", bpo::value<int>(&args.file_duration)->default_value(args.file_duration),
            "The duration (in seconds) of each video file, or the interval between snapshots.")
        ("jpeg-quality", bpo::value<int>(&args.jpeg_quality)->default_value(args.jpeg_quality),
            "Set the quality of the snapshot and thumbnail images in range 0 to 100.")
        ("peer-timeout", bpo::value<int>(&args.peer_timeout)->default_value(args.peer_timeout),
            "The connection timeout (in seconds) after receiving a remote offer")
        ("hw-accel", bpo::bool_switch(&args.hw_accel)->default_value(args.hw_accel),
            "Enable hardware acceleration by sharing DMA buffers between the decoder, "
            "scaler, and encoder to reduce CPU usage.")
        ("no-adaptive", bpo::bool_switch(&args.no_adaptive)->default_value(args.no_adaptive),
            "Disable WebRTC's adaptive resolution scaling. When enabled, "
            "the output resolution will remain fixed regardless of network or device conditions.")
        ("enable-ipc", bpo::bool_switch(&args.enable_ipc)->default_value(args.enable_ipc),
            "Enable IPC relay using a WebRTC DataChannel, lossy (UDP-like) or reliable (TCP-like) based on client preference.")
        ("ipc-channel",  bpo::value<std::string>(&args.ipc_channel)->default_value(args.ipc_channel),
            "IPC channel mode: both, lossy, reliable")
        ("socket-path", bpo::value<std::string>(&args.socket_path)->default_value(args.socket_path),
            "Specifies the Unix domain socket path used to bridge messages between "
            "the WebRTC DataChannel and local IPC applications.")
        ("stun-url", bpo::value<std::string>(&args.stun_url)->default_value(args.stun_url),
            "Set the STUN server URL for WebRTC. e.g. `stun:xxx.xxx.xxx`.")
        ("turn-url", bpo::value<std::string>(&args.turn_url)->default_value(args.turn_url),
            "Set the TURN server URL for WebRTC. e.g. `turn:xxx.xxx.xxx:3478?transport=tcp`.") 
        ("turn-username", bpo::value<std::string>(&args.turn_username)->default_value(args.turn_username),
            "Set the TURN server username for WebRTC authentication.")
        ("turn-password", bpo::value<std::string>(&args.turn_password)->default_value(args.turn_password),
            "Set the TURN server password for WebRTC authentication.")
        ("use-mqtt", bpo::bool_switch(&args.use_mqtt)->default_value(args.use_mqtt),
            "Use MQTT to exchange sdp and ice candidates.")
        ("mqtt-host", bpo::value<std::string>(&args.mqtt_host)->default_value(args.mqtt_host),
            "Set the MQTT server host.")
        ("mqtt-port", bpo::value<int>(&args.mqtt_port)->default_value(args.mqtt_port), "Set the MQTT server port.")
        ("mqtt-username", bpo::value<std::string>(&args.mqtt_username)->default_value(args.mqtt_username),
            "Set the MQTT server username.")
        ("mqtt-password", bpo::value<std::string>(&args.mqtt_password)->default_value(args.mqtt_password),
            "Set the MQTT server password.")
        ("use-whep", bpo::bool_switch(&args.use_whep)->default_value(args.use_whep),
            "Use WHEP (WebRTC-HTTP Egress Protocol) to exchange SDP and ICE candidates.")
        ("http-port", bpo::value<uint16_t>(&args.http_port)->default_value(args.http_port),
            "Local HTTP server port to handle signaling when using WHEP.")
        ("use-websocket", bpo::bool_switch(&args.use_websocket)->default_value(args.use_websocket),
            "Enables the WebSocket client to connect to the SFU server.")
        ("use-tls", bpo::bool_switch(&args.use_tls)->default_value(args.use_tls),
            "Use TLS for the WebSocket connection. Use it when connecting to a `wss://` URL.")
        ("ws-host", bpo::value<std::string>(&args.ws_host)->default_value(args.ws_host),
            "The WebSocket host address of the SFU server.")
        ("ws-room", bpo::value<std::string>(&args.ws_room)->default_value(args.ws_room),
            "The room name to join on the SFU server.")
        ("ws-key", bpo::value<std::string>(&args.ws_key)->default_value(args.ws_key),
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

    if (args.sub_height > 0 && args.sub_width > 0) {
        if (args.sub_width > args.width || args.sub_height > args.height) {
            args.sub_width = args.width;
            args.sub_height = args.height;
            std::cout << "Sub stream resolution should not be larger than main stream. "
                      << "Set to " << args.sub_width << "x" << args.sub_height << std::endl;
        }
        args.num_streams += 1;
        std::cout << "Sub stream is enabled with resolution: " << args.sub_width << "x"
                  << args.sub_height << std::endl;
    } else {
        args.record_stream_idx = 0;
        args.live_stream_idx = 0;
        args.ai_stream_idx = 0;
        std::cout << "Sub stream is not enabled." << std::endl;
    }

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

#if defined(USE_LIBCAMERA_CAPTURE)
    args.sharpness = std::clamp(args.sharpness, 0.0f, 15.99f);
    args.contrast = std::clamp(args.contrast, 0.0f, 15.99f);
    args.brightness = std::clamp(args.brightness, -1.0f, 1.0f);
    args.saturation = std::clamp(args.saturation, 0.0f, 15.99f);
    args.ev = std::clamp(args.ev, -10.0f, 10.0f);
    args.shutter.set(args.shutter_);
    args.ae_metering_mode = ParseEnum(ae_metering_table, args.ae_metering);
    args.ae_mode = ParseEnum(exposure_table, args.exposure);
    args.awb_mode = ParseEnum(awb_table, args.awb);

    if (sscanf(args.awbgains.c_str(), "%f,%f", &args.awb_gain_r, &args.awb_gain_b) != 2) {
        throw std::runtime_error("Invalid AWB gains");
    }

    args.denoise_mode = ParseEnum(denoise_table, args.denoise);

    if (args.tuning_file != "-") {
        setenv("LIBCAMERA_RPI_TUNING_FILE", args.tuning_file.c_str(), 1);
    }

    args.af_mode = ParseEnum(afMode_table, args.autofocus_mode);
    args.af_range_mode = ParseEnum(afRange_table, args.af_range);
    args.af_speed_mode = ParseEnum(afSpeed_table, args.af_speed);

    if (sscanf(args.af_window.c_str(), "%f,%f,%f,%f", &args.af_window_x, &args.af_window_y,
               &args.af_window_width, &args.af_window_height) != 4) {
        args.af_window_x = args.af_window_y = args.af_window_width = args.af_window_height = 0;
    }

    float f = 0.0;
    if (std::istringstream(args.lens_position_) >> f) {
        args.lens_position = f;
    } else if (args.lens_position_ == "default") {
        args.set_default_lens_position = true;
    } else if (!args.lens_position_.empty()) {
        throw std::runtime_error("Invalid lens position: " + args.lens_position_);
    }
#endif

    args.jpeg_quality = std::clamp(args.jpeg_quality, 0, 100);

    args.record_mode = ParseEnum(record_mode_table, args.record);
    args.ipc_channel_mode = ParseEnum(ipc_mode_table, args.ipc_channel);

    ParseDevice(args);
}

void Parser::ParseDevice(Args &args) {
    size_t pos = args.camera.find(':');
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid camera string: " + args.camera +
                                 ". Expected format: libcamera:<id> or v4l2:<id>");
    }

    std::string prefix = args.camera.substr(0, pos);
    std::string id = args.camera.substr(pos + 1);

    try {
        args.camera_id = std::stoi(id);
    } catch (const std::exception &e) {
        throw std::runtime_error("Invalid camera ID: " + id);
    }

    if (prefix == "libcamera") {
#if defined(USE_LIBCAMERA_CAPTURE)
        args.use_libcamera = true;
        args.format = V4L2_PIX_FMT_YUV420;
        std::cout << "Using libcamera, ID: " << args.camera_id << std::endl;
#elif defined(JETSON_PLATFORM)
        throw std::runtime_error("Jetson does not support libcamera. Use v4l2:<id> instead.");
#else
        throw std::runtime_error("libcamera is not supported on this platform.");
#endif

    } else if (prefix == "libargus") {
#if defined(USE_LIBARGUS_CAPTURE)
        args.use_libargus = true;
        args.format = V4L2_PIX_FMT_YUV420;
#elif defined(RPI_PLATFORM)
        throw std::runtime_error("Raspberry Pi does not support libargus. Use v4l2:<id> instead.");
#else
        throw std::runtime_error("libargus is not supported on this platform.");
#endif
    } else if (prefix == "v4l2") {
        args.format = ParseEnum(v4l2_fmt_table, args.v4l2_format);
        std::cout << "Using V4L2, ID: " << args.camera_id << std::endl;
        std::cout << "V4L2 format: " << args.v4l2_format << std::endl;

    } else {
        throw std::runtime_error("Unknown camera type: " + prefix +
                                 ". Expected 'libcamera', 'libargus' or 'v4l2'");
    }
}
