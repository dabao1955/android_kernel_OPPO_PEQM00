#!/bin/bash

set -eu
test -d out || mkdir out -p -v
test -d ../vnd || mkdir vnd -p -v
test -d ../vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_valhall/mali-r25p0 || cp -r -v ../vendor/mediatek/kernel_modules/gpu/gpu_mali/mali_valhall/mali-r25p0 drivers/gpu/mediatek/gpu_mali/mali_valhall/

BUILD_CROSS_COMPILE=$HOME/aarch64-linux-android-4.9/bin/aarch64-linux-android-
CLANG_PATH=$HOME/clang/bin
CROSS_COMPILE_ARM32=$HOME/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-

CLANG_TRIPLE=aarch64-linux-gnu-

export ARCH=arm64
export PATH=${CLANG_PATH}:${PATH}

make -j$(nproc) -C $(pwd) O=$(pwd)/out CROSS_COMPILE=$BUILD_CROSS_COMPILE CLANG_TRIPLE=$CLANG_TRIPLE CROSS_COMPILE_ARM32=$CROSS_COMPILE_ARM32 \
    CC=clang \
    vendor/full/r6p_nodtb_defconfig

A=$(date +%s)
make -j$(nproc) -C $(pwd) O=$(pwd)/out CROSS_COMPILE=$BUILD_CROSS_COMPILE CLANG_TRIPLE=$CLANG_TRIPLE CROSS_COMPILE_ARM32=$CROSS_COMPILE_ARM32 \
    CC=clang \
    -Werror \
    2>&1 | tee build.txt
B=$(date +%s)
C=$(expr $B - $A)
#echo $(expr $C / 60)
echo "kernel finish" $(expr $C / 60)"min"$(expr $C % 60)"s"
#cp out/arch/arm64/boot/Image $(pwd)/Image
