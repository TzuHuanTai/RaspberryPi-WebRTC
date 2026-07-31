# Set up an arm64 environment
Since the compilation of libraries is quite heavy, prepare another Linux environment rather than Raspberry Pi to compile all required libs. If the CPU is an `x86_64` architecture, please set up an `arm64` emulated environment as below.

## Setup filesystem for arm64
```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu qemu-user-static debootstrap
sudo debootstrap --arch arm64 bullseye arm64_bullseye http://ftp.uk.debian.org/debian
```

## Enter the filesystem
```bash
sudo chroot arm64_bullseye /bin/bash
apt-get update
apt-get install -y git lsb-release sudo curl pkg-config
```

## Add new user
```bash
useradd -m pi
echo 'pi ALL=NOPASSWD: ALL' >> /etc/sudoers
cd /home/pi
su - pi
bash
```
