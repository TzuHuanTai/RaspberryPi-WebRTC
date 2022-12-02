﻿# RaspberryPi_WebRTC

Step of using signalr as the webrtc signaling server

1. Build the native webrtc on Ubuntu 20.04 64bit
2. Build the SignalR-Client with boringssl
3. Create .net [signalr server hub](https://github.com/TzuHuanTai/FarmerAPI/blob/master/FarmerAPI/Hubs/SignalingServer.cs)
4. Use signalr-client to exchange ice/sdp information with webrtc.lib.
5. Receive camera frames via `ioctl` and send its to `AdaptedVideoTrackSource` in webrtc.lib 

## Environment
* RaspberryPi 3B + Raspberry Pi Camera v1.3
* RaspberryPi OS 64bit
* [clang 12+](https://github.com/llvm/llvm-project/releases)
* `boringssl` replace `openssl`

## Summary
* Latency is about 0.2 seconds delay.
* Temperatures up to 60~65°C.
* CPU is ~60% at 1280x720 30fps.

| Codec | Source format | Resolution | FPS | CPU  |  Latency  |  Temperature |
| :---: | :-----------: | :--------: | :-: | :--: | :-------: | :---------: |
|  VP8  |     MJPEG     |  1280x720  |  30 | ~60% | 200~250ms |   60~65°C   |
|  VP8  |     MJPEG     |   640x480  |  60 | ~60% | 190~220ms |             |
|  VP8  |     MJPEG     |   320x240  |  60 | ~30% | 120~140ms |             |
|  H264 |     MJPEG     |  1280x720  |  30 | ~25% | 390~530ms |             |
|  H264 |     MJPEG     |   640x480  |  30 | ~20% | 190~200ms |             |
|  H264 |     MJPEG     |   320x240  |  60 | ~20% |<b>70~130ms|             |
|  H264 |    YUV420     |  1280x720  |  15 | ~20% | 300~350ms |             |
|  H264 |    YUV420     |   640x480  |  15 | ~20% | 200~220ms |             |
|  H264 |    YUV420     |   320x240  |  30 | ~15% | 190~200ms |             |

![latency](./doc/latency.jpg)

<hr>

# How to use

Compile `libwebrtc.a` and `microsoft-signalr.so`, then install the needed packages befor run makefile
```bash
sudo apt install libboost-program-options-dev libavformat-dev libavcodec-dev libavutil-dev libavdevice-dev libswscale-dev
# v4l2-ctl -d /dev/video11 --set-ctrl=video_bitrate_mode=1,repeat_sequence_header=1,video_bitrate=1500000,h264_minimum_qp_value=2,h264_maximum_qp_value=31,h264_i_frame_period=12,h264_level=9,h264_profile=0
```

## Compile and run
```bash
make -j
./main http://localhost:6080/SignalingServer
```

## Run as Linux Service
1. Set up [PulseAudio](https://wiki.archlinux.org/title/PulseAudio)
*  Modify this line in `/etc/pulse/system.pa`
    ```ini
    load-module module-native-protocol-unix auth-anonymous=1
    ```

* `sudo nano /etc/systemd/system/pulseaudio.service`, config sample:
    ```ini
    [Unit]
    Description= Pulseaudio Daemon
    After=rtkit-daemon.service systemd-udevd.service dbus.service

    [Service]
    Type=simple
    ExecStart=/usr/bin/pulseaudio --system --disallow-exit --disallow-module-loading --log-target=journal
    Restart=always
    RestartSec=10
      
    [Install]
    WantedBy=multi-user.target
    ```
* Enable and run the service
    ```bash
    sudo systemctl enable pulseaudio.service
    sudo systemctl start pulseaudio.service
    ```

2. Set up WebRTC program 
* `sudo nano /etc/systemd/system/webrtc.service`, config sample:
    ```ini
    [Unit]
    Description= the webrtc service need signaling server first
    After=systemd-networkd.service farmer-api.service

    [Service]
    Type=simple
    WorkingDirectory=/home/pi/IoT/RaspberryPi_WebRTC
    ExecStart=/home/pi/IoT/RaspberryPi_WebRTC/main --fps=30 --width=960 --height=720 --signaling_url=http://localhost:6080/SignalingServer --use_h264_hw_encoder=true --use_i420_src=false
    Restart=always
    RestartSec=10
      
    [Install]
    WantedBy=multi-user.target
    ```
* Enable and run the service
    ```bash
    sudo systemctl enable webrtc.service
    sudo systemctl start webrtc.service
    ```

# Build the [native WebRTC](https://webrtc.github.io/webrtc-org/native-code/development/) lib (`libwebrtc.a`)

## Preparations
* Install some dependent packages
    ```bash
    sudo apt remove libssl-dev
    sudo apt install python pulseaudio libpulse-dev build-essential libncurses5 libx11-dev
    pulseaudio --start
    ```

* Clone WebRTC source code
    ```bash
    mkdir webrtc-checkout
    cd ./webrtc-checkout
    fetch --nohooks webrtc
    src/build/linux/sysroot_scripts/install-sysroot.py --arch=arm64
    gclient sync -D
    # git checkout -b local-4896 branch-heads/4896 # for m100(stable) version
    # gclient sync -D --force --reset --with_branch_heads
    ```
* Download llvm
    ```bash
    curl -OL https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.3/clang+llvm-14.0.3-aarch64-linux-gnu.tar.xz
    tar Jxvf clang+llvm-14.0.3-aarch64-linux-gnu.tar.xz
    export PATH=/home/pi/clang+llvm-14.0.3-aarch64-linux-gnu/bin:$PATH
    echo 'export PATH=/home/pi/clang+llvm-14.0.3-aarch64-linux-gnu/bin:$PATH' >> ~/.bashrc
    ```
* Install the Chromium `depot_tools`.
    ``` bash
    sudo apt install git
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
    export PATH=/home/pi/depot_tools:$PATH
    echo 'PATH=/home/pi/depot_tools:$PATH' >> ~/.bashrc
    ```
* Replace ninja in the `depot_tools`.
    ``` bash
    git clone https://github.com/martine/ninja.git;
    cd ninja;
    ./configure.py --bootstrap;
    mv /home/pi/depot_tools/ninja /home/pi/depot_tools/ninja_org;
    cp /home/pi/ninja/ninja /home/pi/depot_tools/ninja;
    ```
* Replace gn in the `depot_tools`.
    ``` bash
    git clone https://gn.googlesource.com/gn;
    cd gn;
    sed -i -e "s/-Wl,--icf=all//" build/gen.py;
    python build/gen.py;
    ninja -C out;
    sudo mv /home/pi/webrtc-checkout/src/buildtools/linux64/gn /home/pi/webrtc-checkout/src/buildtools/linux64/gn_org;
    cp /home/pi/gn/out/gn /home/pi/webrtc-checkout/src/buildtools/linux64/gn;
    sudo mv /home/pi/depot_tools/gn /home/pi/depot_tools/gn_org;
    cp /home/pi/gn/out/gn /home/pi/depot_tools/gn;
    ```

## Compile `libwebrtc.a`

* Build
    ``` bash
    gn gen out/Default64 --args='target_os="linux" target_cpu="arm64" rtc_include_tests=false rtc_use_h264=false use_rtti=true is_component_build=false is_debug=true rtc_build_examples=false use_custom_libcxx=false rtc_use_pipewire=false clang_base_path="/home/pi/clang+llvm-14.0.3-aarch64-linux-gnu" treat_warnings_as_errors=false clang_use_chrome_plugins=false'
    ninja -C out/Default64
    ```
cause    *note: In contrast to the release version, debug version cause frames to be blocked by the video sender.*
* Extract header files
    ```bash
    rsync -amv --include=*/ --include=*.h --include=*.hpp --exclude=* ./ ./include
    ```

# Build the [SignalR-Client-Cpp](https://github.com/aspnet/SignalR-Client-Cpp) lib (`microsoft-signalr.so`)
## Preparations
* `sudo apt-get install build-essential curl git cmake ninja-build golang libpcre3-dev zlib1g-dev`
* Install boringssl
    ```bash
    sudo apt-get install libunwind-dev zip unzip tar
    git clone https://github.com/google/boringssl.git
    cd boringssl
    mkdir build
    cd build
    cmake -GNinja .. -DBUILD_SHARED_LIBS=1
    ninja
    sudo ninja install
    sudo cp -r ../install/include/* /usr/local/include/
    sudo cp ../install/lib/*.a /usr/local/lib/
    sudo cp ../install/lib/*.so /usr/local/lib/
    ```
*  Install [cpprestsdk](https://github.com/Microsoft/cpprestsdk/wiki/How-to-build-for-Linux)
    ```bash
    sudo apt-get install libboost-atomic-dev libboost-thread-dev libboost-system-dev libboost-date-time-dev libboost-regex-dev libboost-filesystem-dev libboost-random-dev libboost-chrono-dev libboost-serialization-dev libwebsocketpp-dev
    git clone https://github.com/Microsoft/cpprestsdk.git casablanca
    cd casablanca
    mkdir build.debug
    cd build.debug
    ```
    because we use boringssl rather than openssl, so need modify `/home/pi/casablanca/Release/cmake/cpprest_find_openssl.cmake:53` from
    ```cmake
    find_package(OpenSSL 1.0.0 REQUIRED)
    ```
    to
    ```cmake
    set(OPENSSL_INCLUDE_DIR "/usr/local/include/openssl" CACHE INTERNAL "")
    set(OPENSSL_LIBRARIES "/usr/local/lib/libssl.a" "/usr/local/lib/libcrypto.a" CACHE INTERNAL "")
    ```
    and replace `-std=c++11` into `-std=c++14` in `Release/CMakeLists.txt`, then
    ``` bash
    cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug
    ```
    then build
    ``` bash
    ninja
    sudo ninja install
    ```

## Compile `microsoft-signalr.so`
1. Clone the source code of SignalR-Client-Cpp
    ```bash
    git clone https://github.com/aspnet/SignalR-Client-Cpp.git
    cd ./SignalR-Client-Cpp/
    git submodule update --init
    ```
2. In `SignalR-Client-Cpp/CMakeLists.txt:60`, replace from
    ```cmake
    find_package(OpenSSL 1.0.0 REQUIRED)
    ```
    into
    ```cmake
    set(OPENSSL_INCLUDE_DIR "/usr/local/include/openssl" CACHE INTERNAL "")
    set(OPENSSL_LIBRARIES "/usr/local/lib/libssl.a" "/usr/local/lib/libcrypto.a" CACHE INTERNAL "")
    ```
    and in `line 17` replace from `-std=c++11` into `-std=c++14` as well.
3. Build
    ``` bash
    cd ..
    mkdir build-release
    cd ./build-release/
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_CPPRESTSDK=true
    cmake --build . --config Release
    sudo make install
    ```
# Reference
* [Version | WebRTC](https://chromiumdash.appspot.com/branches)
* [Building old revisions | WebRTC](https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/building_old_revisions.md)
* [Using a custom clang binary | WebRTC](https://chromium.googlesource.com/chromium/src/+/master/docs/clang.md#using-a-custom-clang-binary)
