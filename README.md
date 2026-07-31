<p align=center>
    <img src="docs/pi_5_latency_demo.gif" alt="Pi 4b latency demo">
</p>

<h1 align="center">
 <p>Raspberry Pi WebRTC</p>
</h1>

<p align="center">
Turn Raspberry Pi or NVIDIA Jetson into a low-latency<b> ~200ms </b> WebRTC streaming platform.
</p>


<p align="center">
    <a href="https://chromium.googlesource.com/external/webrtc/+/branch-heads/7680"><img src="https://img.shields.io/badge/libwebrtc-m146.7680-red.svg" alt="WebRTC Version"></a>
    <img src="https://img.shields.io/github/downloads/TzuHuanTai/RaspberryPi_WebRTC/total.svg?color=yellow" alt="Download">
    <img src="https://img.shields.io/badge/C%2B%2B-20-brightgreen?logo=cplusplus">
    <img src="https://img.shields.io/github/v/release/TzuHuanTai/RaspberryPi_WebRTC?color=blue" alt="Release">
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-purple.svg" alt="License Apache"></a>
</p>

<hr>

- Native WebRTC with hardware/software encoding
- Support snapshot, recording and [broadcasting](https://youtu.be/fuJ_EzwmlPM?si=66H8CgUIKo85leHI)
- Remote control and IoT messaging via WebRTC DataChannel
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

## Requirements

<img src="https://assets.raspberrypi.com/static/51035ec4c2f8f630b3d26c32e90c93f1/2b8d7/zero2-hero.webp" height="96">

- **Raspberry Pi (Zero/3/4/5)** or **NVIDIA Jetson (Nano/NX/Orin)** 
- CSI or USB camera (supports libcamera, libargus or V4L2)

# Quick Start for Pi

Check out the [tutorial video](https://youtu.be/g5Npb6DsO-0) or follow these steps.

### 1. Flash Raspberry Pi OS

Use [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to flash **Lite OS** to SD card.

### 2. Install Dependencies

```bash
sudo apt update
sudo apt install libmosquitto1 pulseaudio libavformat61 libswscale8 libyaml-cpp0.8
```

### 3. Download Binary

Get the latest [release binary](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/releases) .
```bash
wget https://github.com/TzuHuanTai/RaspberryPi-WebRTC/releases/latest/download/pi-webrtc_raspios-trixie-arm64.tar.gz
tar -xzf pi-webrtc_raspios-trixie-arm64.tar.gz
```

### 4. MQTT Signaling

Use [HiveMQ](https://www.hivemq.com), [EMQX](https://www.emqx.com/en), or a [self-hosted](docs/SETUP_MOSQUITTO.md) broker.

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
> Remove `--hw-accel` for Pi 5 or others without hardware encoder.

# Documentation

- [Configuration](docs/CONFIGURATION.md) — every flag, the YAML config file
- [Camera and Encoding](docs/CAMERA_AND_ENCODING.md) — camera backends, hardware and software encoding
- [Signaling](docs/SIGNALING.md) — MQTT, WHEP, and SFU
- [Recording](docs/RECORDING.md) — files on disk, rotation, on-demand capture
- [Architecture](docs/ARCHITECTURE.md) — how the pieces fit together
- [Building](docs/BUILD.md) — building from source

## Advanced Usage

- [Broadcasting a Live Stream to 1,000+ Viewers via SFU](docs/ADVANCED.md#broadcasting-a-live-stream-to-1000-viewers-via-sfu)
- [Using the Legacy V4L2 Driver](docs/ADVANCED.md#using-the-legacy-v4l2-driver)
- [Running as a Linux Service](docs/ADVANCED.md#running-as-a-linux-service)
- [Two-way Audio Communication](docs/ADVANCED.md#two-way-audio-communication)
- [Two-way DataChannel Messaging](docs/ADVANCED.md#two-way-datachannel-messaging)
- [Stream AI or Any Custom Feed to a Virtual Camera](docs/ADVANCED.md#stream-ai-or-any-custom-feed-to-a-virtual-camera)
- [WHEP with Nginx Proxy](docs/ADVANCED.md#whep-with-nginx-proxy)
- [Using the WebRTC Camera in Home Assistant](docs/ADVANCED.md#using-the-webrtc-camera-in-home-assistant)
