# Build the [mosquitto](https://github.com/eclipse/mosquitto/tree/15292b20b0894ec7c5c3d47e4b22ee9d89f91132) lib (`mosquitto.a`,`mosquitto.so` and header files )
## Preparations
* Install some dependent packages
    ```bash
    sudo apt-get install build-essential curl git cmake ninja-build golang libpcre3-dev zlib1g-dev libcjson-dev
    ```
## Compile `mosquitto.a`
1. Clone the source code and build
    ```bash
    wget https://mosquitto.org/files/source/mosquitto-2.0.18.tar.gz
    tar -zxvf mosquitto-2.0.18.tar.gz
    cd mosquitto-2.0.18
    make WITH_STATIC_LIBRARIES=yes
    sudo make install
    ```

    you can find the static libray in

    ```
    mosquitto-2.0.18/lib/libmosquitto.a
    ```

    the dyanimc library in

    ```
    /usr/local/lib/libmosquittopp.so.1
    /usr/local/lib/libmosquitto.so.1
    /usr/local/lib/mosquitto_dynamic_security.so
    ```

    the header files in

    ```
    /usr/local/include/mosquitto*.h
    ```

2. Use scp or other ways to transfer headers and libs to the Raspberry Pi, then move them to `/usr/local`

    e.g. 
    ```bash

    # header files
    tar -czf mosquitto_headers.tar.gz -C /usr/local/include/ mosquitto*.h
    scp mosquitto_headers.tar.gz pi@192.168.x.x:/home/pi
    ssh pi@192.168.x.x 'tar -xzf /home/pi/mosquitto_headers.tar.gz -C /home/pi'
    ssh pi@192.168.x.x 'sudo mv /home/pi/mosquitto*.h /usr/local/include/'

    # .a files
    scp mosquitto-2.0.18/lib/libmosquitto.a pi@192.168.x.x:/home/pi
    sudo mv /home/pi/libmosquitto.a /usr/local/lib

    # .so files
    scp mosquitto-2.0.18/lib/libmosquittopp.so.1 pi@192.168.x.x:/home/pi
    sudo mv /home/pi/libmosquittopp.so.1 /usr/local/lib
    scp mosquitto-2.0.18/lib/libmosquittopp.so.1 pi@192.168.x.x:/home/pi
    sudo mv /home/pi/libmosquittopp.so.1 /usr/local/lib
    scp mosquitto-2.0.18/lib/mosquitto_dynamic_security.so pi@192.168.x.x:/home/pi
    sudo mv /home/pi/mosquitto_dynamic_security.so /usr/local/lib
    ```
