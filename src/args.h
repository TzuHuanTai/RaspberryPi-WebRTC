#ifndef ARGS_H_
#define ARGS_H_

#include <string>

#include <linux/videodev2.h>

struct Args {
    int fps = 30;
    int width = 640;
    int height = 480;
    int rotation_angle = 0;
    int sample_rate = 44100;
    bool hw_accel = false;
    uint32_t format = V4L2_PIX_FMT_MJPEG;
    std::string v4l2_format = "mjpeg";
    std::string device = "";
    std::string uid = "";
    std::string stun_url = "stun:stun.l.google.com:19302";
    std::string turn_url = "";
    std::string turn_username = "";
    std::string turn_password = "";
#if USE_MQTT_SIGNALING
    int mqtt_port = 1883;
    std::string mqtt_host = "localhost";
    std::string mqtt_username = "";
    std::string mqtt_password = "";
#elif USE_SIGNALR_SIGNALING
    std::string signaling_url = "http://localhost:5000/SignalingServer";
#endif
    std::string record_path = "";
    int max_files = 120;
};

#endif // ARGS_H_
