# Build the project

## Preparation
1. Follow [SETUP_ARM64_ENV](SETUP_ARM64_ENV.md) to prepare an arm64 env for compilation. (Optional)
2. Follow [BUILD_WEBRTC](BUILD_WEBRTC.md) to compile a `libwebrtc.a`.
3. Prepare the MQTT development library.
    * Follow [BUILD_MOSQUITTO](BUILD_MOSQUITTO.md) to compile `mosquitto`.
    * Install the lib from official repo [[tutorial](https://repo.mosquitto.org/debian/README.txt)]. (recommended)
4. Install essential packages
    ```bash
    sudo apt install cmake clang clang-format mosquitto-dev libboost-program-options-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libpulse-dev libasound2-dev libjpeg-dev libcamera-dev libmosquitto-dev protobuf-compiler libprotobuf-dev
    ```
5. Copy the [nlohmann/json.hpp](https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp) to `/usr/local/include`
    ```bash
    sudo mkdir -p /usr/local/include/nlohmann
    sudo curl -L https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp -o /usr/local/include/nlohmann/json.hpp
    ```

## Compile and run

| <div style="width:200px">Command line</div> | Default     | Options      |
| --------------------------------------------| ----------- | ------------ |
| -DPLATFORM         | raspberrypi            | jetson, raspberrypi        |
| -DBUILD_TEST       |                        | http_server, recorder, mqtt, v4l2_capture, v4l2_encoder, v4l2_decoder, v4l2_scaler, unix-socket, libcamera, libargus |
| -DCMAKE_BUILD_TYPE | Debug                  | Debug, Release             |

Build on raspberry pi and it'll output a `pi-webrtc` file in `/build`.
```bash
mkdir build
cd build
cmake .. -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_BUILD_TYPE=Release
make -j
```

Run `pi-webrtc` to start the service.
```bash
./pi-webrtc --camera=libcamera:0 --fps=30 --width=1280 --height=720 --use-mqtt --mqtt-host=<hostname> --mqtt-port=1883 --mqtt-username=<username> --mqtt-password=<password> --hw-accel
```
