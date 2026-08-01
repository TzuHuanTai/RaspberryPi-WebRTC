# Commercial Version

`pi-webrtc` is open source under [Apache-2.0](../LICENSE). A few features are developed and
maintained separately and are available under a commercial license.

Throughout this documentation, anything marked with a <sup>[\*](COMMERCIAL.md#licensing)</sup>
links back to this page.

## What's Included

### Object Detection

On-device YOLO inference through TensorRT, with the results drawn onto the outgoing video
stream. Detection runs on a dedicated sub-stream (see [Configuration](CONFIGURATION.md#sub-stream))
so the main stream keeps its full resolution, and frames move between the camera, the
detector, and the encoder as CUDA/EGL handles without a copy back to the CPU.

| Flag | Description |
|---|---|
| `--detector-model` | Path to the TensorRT engine file. |
| `--detector-labels` | Class-name list, one per line. Defaults to the COCO 80 classes. |
| `--detector-confidence` | Minimum confidence threshold, `0.0`–`1.0`. |

### Object Tracking

Multi-object tracking on top of the detector, using NVIDIA's NvMOT library. Both the
correlation-filter tracker (NvDCF) and the re-identification tracker (DeepSORT) are
supported, selected by config file. Clients can toggle the overlay at runtime with the
`TOGGLE_TRACKING` DataChannel command.

| Flag | Description |
|---|---|
| `--tracker-config` | Path to an NvMOT YAML config, e.g. `config/tracker_NvDCF.yml` or `config/tracker_NvDeepSORT.yml`. |

### Multi-camera Capture

Drive several cameras from a single `pi-webrtc` process. Each camera is declared in the
`cameras:` list of a [YAML config file](CONFIGURATION.md#multi-camera), inherits the global
settings, and can override its own resolution, sub-stream, and recording directory — or opt
out of WebRTC or recording entirely.

### LiveKit Token Service

The open-source build expects a LiveKit access token to be supplied for it. The commercial
build can obtain one itself, either by calling a token service over HTTP (`--token-url`) or
by signing tokens locally from an API key/secret pair (`--livekit-secret`), with automatic renewal
before expiry.

| Flag | Description |
|---|---|
| `--token-url` | URL of the token service issuing LiveKit access tokens. |
| `--livekit-secret` | LiveKit API secret, paired with `--livekit-key`. Signs tokens on-device; `--token-url` is then unused. |
| `--token-ttl` | Requested token lifetime in seconds. |

## Hardware Requirements

| Feature | Requirement |
|---|---|
| Object detection | NVIDIA Jetson with TensorRT and CUDA |
| Object tracking | NVIDIA Jetson with DeepStream (`libnvds_nvmultiobjecttracker.so`) |
| Multi-camera | Any supported platform |
| LiveKit token service | Any supported platform |

Detection and tracking are built against TensorRT, CUDA, and DeepStream, so they are
Jetson-only. They are not available on Raspberry Pi.

## Licensing

Licensing is arranged directly, so that terms can be matched to how the software is actually
deployed — a single device, a fleet, or an integration into a product you ship. Get in touch
and I'll send the current pricing and terms.

Worth mentioning when you write, so the first reply is a useful one:

- Which of the features above you need
- Target hardware and roughly how many devices
- Whether it is for internal use or for a product you distribute

## Contact

For licensing, a dedicated SFU environment, or integration questions:

**tzu.huan.tai@gmail.com**
