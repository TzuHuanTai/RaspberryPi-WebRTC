<p align=center>
    <img src="doc/pi_5_latency_demo.gif" alt="Pi 4b latency demo">
</p>

<h1 align="center">
 <p>Raspberry Pi WebRTC</p>
</h1>

<p align="center">
Turn your Raspberry Pi into a real-time,<b> ~200ms low-latency</b> WebRTC camera.
</p>


<p align="center">
    <a href="https://chromium.googlesource.com/external/webrtc/+/branch-heads/5790"><img src="https://img.shields.io/badge/libwebrtc-m115.5790-red.svg" alt="WebRTC Version"></a>
    <img src="https://img.shields.io/github/downloads/TzuHuanTai/RaspberryPi_WebRTC/total.svg?color=yellow" alt="Download">
    <img src="https://img.shields.io/badge/C%2B%2B-20-brightgreen?logo=cplusplus">
    <img src="https://img.shields.io/github/v/release/TzuHuanTai/RaspberryPi_WebRTC?color=blue" alt="Release">
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-purple.svg" alt="License Apache"></a>
</p>

<hr>

- Snapshot/video recording and WebRTC DataChannel messaging
- Supports P2P, [SFU broadcasting](https://youtu.be/fuJ_EzwmlPM?si=66H8CgUIKo85leHI), and [Home Assistant](https://www.home-assistant.io) integration
- Signaling options:

  **MQTT**
    * [picamera.js](https://www.npmjs.com/package/picamera.js)
    * [picamera-react-native](https://www.npmjs.com/package/picamera-react-native)
    * [picamera-web](https://app.picamera.live)
    * [picamera-app](https://github.com/TzuHuanTai/picamera-app) - Android

  **[WHEP](https://www.ietf.org/archive/id/draft-ietf-wish-whep-02.html)**
    * [Home Assistant WebRTC Camera](https://github.com/AlexxIT/WebRTC)
    * [eyevinn/webrtc-player](https://www.npmjs.com/package/@eyevinn/webrtc-player)

  **WebSocket**
    * [picamera.js](https://github.com/TzuHuanTai/picamera.js?tab=readme-ov-file#watch-videos-via-the-sfu-server)  - SFU signaling & broadcast

# Quick Start

Check out the [tutorial video](https://youtu.be/g5Npb6DsO-0) or follow these steps.

## Requirements

<img src="https://assets.raspberrypi.com/static/51035ec4c2f8f630b3d26c32e90c93f1/2b8d7/zero2-hero.webp" height="96">

- Raspberry Pi Zero 2W, 3B, 4B, or 5
- CSI or USB camera (supports libcamera or V4L2)

## Setup

### 1. Flash Raspberry Pi OS

Use [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to flash **Lite OS** (or full OS) to SD card.

### 2. Install Dependencies

```bash
sudo apt update
sudo apt isnstall libcamera0.5 libmosquitto1 pulseaudio libavformat59 libswscale6 libprotobuf32
```

### 3. Download App

Get the latest [release binary](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/releases) .
```bash
wget https://github.com/TzuHuanTai/RaspberryPi-WebRTC/releases/latest/download/pi-webrtc-v1.1.1_raspios-bookworm-arm64.tar.gz
tar -xzf pi-webrtc-v1.1.1_raspios-bookworm-arm64.tar.gz
```

### 4. MQTT Signaling

Use [HiveMQ](https://www.hivemq.com), [EMQX](https://www.emqx.com/en), or a [self-hosted](doc/SETUP_MOSQUITTO.md) broker.

> [!TIP]
> **MQTT** lets your Pi camera and client exchange WebRTC connection info.
**WHEP** doesn’t need a broker but requires a public hostname.

## Run the App

![preview_demo](https://github.com/user-attachments/assets/d472b6e0-8104-4aaf-b02b-9925c5c363d0)

- Open [picamera-web](https://app.picamera.live), add MQTT settings, and create a `UID`.
- Run the command on your Pi:
    ```bash
    ./pi-webrtc \
        --camera=libcamera:0 \
        --fps=30 \
        --width=1280 \
        --height=960 \
        --use-mqtt \
        --mqtt-host=your.mqtt.cloud \
        --mqtt-port=8883 \
        --mqtt-username=hakunamatata \
        --mqtt-password=Wonderful \
        --uid=your-custom-uid \
        --no-audio \
        --hw-accel # Only Pi Zero 2W, 3B, 4B support hw encoding
    ```

> [!IMPORTANT]
> Remove `--hw-accel` for Pi 5 or others without hardware encoder..

# [Advanced Usage](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/wiki/Advanced-Settings)

- [Broadcasting a Live Stream to 1,000+ Viewers via SFU](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#broadcasting-a-live-stream-to-1000-viewers-via-sfu)
- [Using the V4L2 Driver](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#using-the-legacy-v4l2-driver) (for USB cameras)
- [Running as a Linux Service](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#running-as-a-linux-service)
- [Recording](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#recording)
- [Two-way Audio Communication](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#two-way-audio-communication)
- [Two-way DataChannel Messaging](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#two-way-datachannel-messaging)
- [Stream AI or Any Custom Feed to a Virtual Camera](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings##stream-ai-or-any-custom-feed-to-a-virtual-camera)
- [WHEP with Nginx proxy](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#whep-with-nginx-proxy)
- [Using the WebRTC Camera in Home Assistant](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#using-the-webrtc-camera-in-home-assistant)
