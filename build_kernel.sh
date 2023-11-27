#!/bin/bash

set -eux
test -d out || mkdir out -p -v

BUILD_CROSS_COMPILE=$HOME/aarch64-linux-android-4.9/bin/aarch64-linux-android-
CLANG_PATH=$HOME/clang/bin
CROSS_COMPILE_ARM32=$HOME/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-

CLANG_TRIPLE=aarch64-linux-gnu-

export ARCH=arm64
export PATH=${CLANG_PATH}:${PATH}
export USE_CCACHE=1

make -j$(nproc) -C $(pwd) O=$(pwd)/out CROSS_COMPILE=$BUILD_CROSS_COMPILE CLANG_TRIPLE=$CLANG_TRIPLE CROSS_COMPILE_ARM32=$CROSS_COMPILE_ARM32 \
    CC=clang \
    vendor/full/r6_nodtb_defconfig

A=$(date +%s)
make -j$(nproc) -C $(pwd) O=$(pwd)/out CROSS_COMPILE=$BUILD_CROSS_COMPILE CLANG_TRIPLE=$CLANG_TRIPLE CROSS_COMPILE_ARM32=$CROSS_COMPILE_ARM32 \
    CC="ccache clang" \
    -Werror \
    2>&1 | tee build.txt
B=$(date +%s)
C=$(expr $B - $A)
#echo $(expr $C / 60)
echo "kernel finish" $(expr $C / 60)"min"$(expr $C % 60)"s"
#cp out/arch/arm64/boot/Image $(pwd)/Image

