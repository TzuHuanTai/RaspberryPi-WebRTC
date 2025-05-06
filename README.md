<h1 align="center">
    Raspberry Pi WebRTC
</h1>

<p align="center">
    <a href="https://chromium.googlesource.com/external/webrtc/+/branch-heads/5790"><img src="https://img.shields.io/badge/libwebrtc-m115.5790-red.svg" alt="WebRTC Version"></a>
    <img src="https://img.shields.io/github/downloads/TzuHuanTai/RaspberryPi_WebRTC/total.svg?color=yellow" alt="Download">
    <img src="https://img.shields.io/badge/C%2B%2B-20-brightgreen?logo=cplusplus">
    <img src="https://img.shields.io/github/v/release/TzuHuanTai/RaspberryPi_WebRTC?color=blue" alt="Release">
    <a href="https://opensource.org/licenses/Apache-2.0"><img src="https://img.shields.io/badge/License-Apache_2.0-purple.svg" alt="License Apache"></a>
</p>

<p align=center>
    <img src="doc/pi_4b_latency_demo.gif" alt="Pi 4b latency demo">
</p>

Turn your Raspberry Pi into a low-latency security camera using the hardware encoder and WebRTC.

- Supports real-time adjustment of camera parameters and video recording download.
- Support [multiple users](doc/pi_4b_users_demo.gif) for simultaneous live streaming.
- Support signaling 

  **MQTT**
    * [picamera.js](https://www.npmjs.com/package/picamera.js)
    * [picamera-react-native](https://www.npmjs.com/package/picamera-react-native)
    * [picamera-app](https://github.com/TzuHuanTai/picamera-app) *(demo application)*
    * [picamera-web](https://app.picamera.live) *(demo application)*

  **[WHEP](https://www.ietf.org/archive/id/draft-ietf-wish-whep-02.html)**
    * [Home Assistant](https://www.home-assistant.io)
    * [eyevinn/webrtc-player](https://www.npmjs.com/package/@eyevinn/webrtc-player)

  **WebSocket**
    * [picamera.js](https://github.com/TzuHuanTai/picamera.js?tab=readme-ov-file#watch-videos-via-the-sfu-server) *(SFU signaling & broadcasting)*

- 🎥 [Watch demo](https://www.youtube.com/watch?v=JZ5bcSAsXog)


# Quick Start

To set up the environment, please check out the [tutorial video](https://youtu.be/g5Npb6DsO-0) or the steps below.

## Hardware Requirements

<img src="https://assets.raspberrypi.com/static/51035ec4c2f8f630b3d26c32e90c93f1/2b8d7/zero2-hero.webp" height="96">

* Raspberry Pi (Zero 2W/3B/3B+/4B/5).
* CSI or USB Camera Module.

## Environment Setup

### 1. Install Raspberry Pi OS

Use the [Raspberry Pi Imager](https://www.raspberrypi.com/software/) to install Raspberry Pi Lite OS on your microSD card.

<details>
  <summary>
    <b>💡 Can I use a regular Raspberry Pi OS, or does it have to be Lite?</b>
  </summary>

> You can use either the Lite or full Raspberry Pi OS (the official recommended versions), but Lite OS is generally more efficient.

</details>

### 2. Install Dependencies

```bash
sudo apt update
sudo apt full-upgrade
sudo apt install libmosquitto1 pulseaudio libavformat59 libswscale6
```

### 3. Download & Unpack

Prepare the binary file from [Releases](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/releases).
```bash
wget https://github.com/TzuHuanTai/RaspberryPi_WebRTC/releases/latest/download/pi_webrtc-v1.0.6-rc.3_raspios-bookworm-arm64.tar.gz
tar -xzf pi_webrtc-v1.0.6-rc.3_raspios-bookworm-arm64.tar.gz
```

### 4. Set Up MQTT

You can use a free cloud MQTT service like  [HiveMQ](https://www.hivemq.com) or [EMQX](https://www.emqx.com/en), or set up your own self-hosted broker.

<details>
  <summary>
    <b>💡 Is MQTT registration necessary, and why is MQTT needed?</b>
  </summary>

> MQTT is one option for signaling P2P connection information between your camera and the client UI. WHEP, on the other hand, runs an HTTP service locally and does not require a third-party server. It is only suitable for devices with a public hostname. If you choose to self-host an MQTT server (e.g., [Mosquitto](doc/SETUP_MOSQUITTO.md)) and need to access the signaling server remotely via mobile data, you may need to set up DDNS, port forwarding, and SSL/TLS.

</details>

## Running the Application

* Set up the MQTT settings on your [Pi Camera App](https://github.com/TzuHuanTai/Pi-Camera-App) or [Pi Camera Web](https://app.picamera.live), and create a new device in the **Settings** page to get a `UID`. 
* Run the command based on your network settings and `UID` on the Raspberry Pi:
    ```bash
    ./pi_webrtc \
        --camera=libcamera:0 \
        --fps=30 \
        --width=1280 \
        --height=960 \
        --use_mqtt \
        --mqtt_host=your.mqtt.cloud \
        --mqtt_port=8883 \
        --mqtt_username=hakunamatata \
        --mqtt_password=Wonderful \
        --uid=your-custom-uid \
        --no_audio \
        --hw_accel # Only Pi Zero 2W, 3B, 4B support hw encoding
    ```

> [!IMPORTANT]
> Use `--hw_accel` for Pi Zero 2W, 3B, 3B+, and 4B. Remove for Pi 5 or devices without hardware encoding.

* Go to the Live page to enjoy real-time streaming!

<p align=center>
    <img src="doc/web_live_demo.jpg" alt="Pi 5 live demo on web">
</p>

# [Advance](https://github.com/TzuHuanTai/RaspberryPi_WebRTC/wiki/Advanced-Settings)

- [Broadcasting Live Stream to 1,000+ Viewers via SFU](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#broadcasting-live-stream-to-1000-viewers-via-sfu)
- [Using the V4L2 Driver](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#using-the-legacy-v4l2-driver) (for usb camera)
- [Run as Linux Service](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#run-as-linux-service)
- [Recording](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#recording)
- [Two-way communication](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#two-way-communication)
- [Virtual Camera](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings##virtual-camera)
- [WHEP with Nginx proxy](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#whep-with-nginx-proxy)
- [Use WebRTC Camera in Home Assistant](https://github.com/TzuHuanTai/RaspberryPi-WebRTC/wiki/Advanced-Settings#use-webrtc-camera-in-home-assistant)
