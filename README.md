# android_kernel_OPPO_PEQM00
Support kernelSU kernel source code for MediaTek devices in the OPPO Reno series on ColorOS13.x
# Supported Devices
- [x] OPPO Reno6 (mt6877)

- [x] OPPO Reno6 Pro (mt6893)

- [ ] OPPO Reno5 Pro (mt6889)(~~WIP~~)

- [ ] OPPO Reno7 Pro (mt6893)(~~WIP~~) ~~(no offical way to unlock bootloader)~~

- [ ] OPPO Reno8 (mt6893) ~~(no way to unlock bootloader)~~

- [ ] OPPO Qualcomm Device ~~(This is the kernel source code of the MediaTek device, not the kernel source code of the Qualcomm device. In the future, there may be an entire kernel source code for sm8280 or sm7250 that supports kernelSU.)~~
# Branches
- Readme: rt
- 4.19: default kernel source branch
- 4.19-ksu-dev: kernel source branch with KernelSU
- oplus-vendor: oplus vendor source branch

# Status

[![build kernel - OPPO Reno6](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6877-4.19-noksu.yml/badge.svg)](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6877-4.19-noksu.yml)

[![build KSU kernel - OPPO Reno6](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6877-4.19.yml/badge.svg)](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6877-4.19.yml)

[![build kernel - OPPO Reno6 Pro](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6893-4.19-noksu.yml/badge.svg)](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6893-4.19-noksu.yml)

[![build KSU kernel - OPPO Reno6 Pro](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6893-4.19.yml/badge.svg)](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions/workflows/6893-4.19.yml)

![Downloads](https://img.shields.io/github/downloads/dabao1955/android_kernel_OPPO_PEQM00/total)  

# Quick Start
### Device Enviroment
at least 2g memory free and 8g free disk space.
### Setup Enviroment
Required dependencies for installation for Debian/Ubuntu
```bash
sudo apt-get install libncurses5-dev libncurses-dev libssl-dev device-tree-compiler bc cpio lib32ncurses5-dev lib32z1 build-essential binutils bc bison build-essential ccache curl flex g++-multilib gcc-multilib git gnupg gperf imagemagick lib32ncurses5-dev lib32readline-dev lib32z1-dev liblz4-tool libncurses5 libncurses5-dev libsdl1.2-dev libssl-dev libxml2 libxml2-utils lzop pngcrush rsync schedtool squashfs-tools xsltproc zip zlib1g-dev git
```
### Pull toolchain for your home folder
run
```
git clone https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9 aarch64-linux-android-4.9 --depth=1
git clone https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9 arm-linux-androideabi-4.9 --depth=1
wget https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/heads/android12-release/clang-r383902.tar.gz
```
Download kernel source code `from wget instead of git` to your home folder ,and extract kernel source archive and rename to `kernel-4.19`

Run git to get kernel vendor source code
```bash
git clone https://github.com/dabao1955/android_kernel_OPPO_PEQM00 -b oplus_vendor vendor --depth=1
```
### Extra Steps
Copy vendor folder to your home folder and copy vendor/oplus/kernel_4.19/audio to kernel-4.19/sound/soc/codecs

Run setup.sh to sync kernelsu 

edit build_kernel.sh to change config.

Finally,run `build_kernel.sh` before you into the kernel-4.19 folder.

# Download Built Kernel

[here](https://github.com/dabao1955/android_kernel_OPPO_PEQM00/actions)
