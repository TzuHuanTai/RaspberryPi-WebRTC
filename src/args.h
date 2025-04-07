#ifndef ARGS_H_
#define ARGS_H_

#include <cstdint>
#include <string>

#include <linux/videodev2.h>

struct Args {
    int fps = 30;
    int width = 640;
    int height = 480;
    int cameraId = 0;
    int jpeg_quality = 30;
    int rotation_angle = 0;
    int sample_rate = 44100;
    int peer_timeout = 10;
    int segment_duration = 60;
    bool no_audio = false;
    bool hw_accel = false;
    bool use_libcamera = false;
    bool use_mqtt = false;
    bool use_whep = false;
    bool use_websocket = false;
    bool fixed_resolution = false;
    uint32_t format = V4L2_PIX_FMT_MJPEG;
    std::string v4l2_format = "mjpeg";
    std::string camera = "libcamera:0";
    std::string uid = "";
    std::string stun_url = "stun:stun.l.google.com:19302";
    std::string turn_url = "";
    std::string turn_username = "";
    std::string turn_password = "";
    std::string record_path = "";

    // mqtt signaling
    int mqtt_port = 1883;
    std::string mqtt_host = "localhost";
    std::string mqtt_username = "";
    std::string mqtt_password = "";

    // http signaling
    uint16_t http_port = 8080;

    // websocket signaling
    bool use_tls = false;
    std::string ws_host = "192.168.4.21";
    std::string ws_token =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJleHAiOjE3NzQ3NDU2MzksImlzcyI6IkFQSVduUVRzNHRtVVp2QSIsIm5iZiI6MTc0MzIwOTYzOSwic3ViIjoiZj"
        "JkNzRiOTUtMmYxNi00ODRiLTg3NjctYThjNWY3NzFlZWY2IiwidmlkZW8iOnsiY2FuUHVibGlzaCI6dHJ1ZSwiY2Fu"
        "UHVibGlzaERhdGEiOnRydWUsImNhblN1YnNjcmliZSI6ZmFsc2UsInJvb20iOiJkZXZpY2UtMSIsInJvb21Kb2luIj"
        "p0cnVlfX0.o7e-gjkqfMpeDjATwaWWLKUWPa8RlaoIOcuw3p8FxQk";
};

#endif // ARGS_H_
