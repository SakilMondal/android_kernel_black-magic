#!/bin/bash
#
# Script to compile BlackMagic Image for Zenfone Max (Z010D)
#
export ARCH=arm64
export SUBARCH=arm64
export CROSS_COMPILE=$(pwd)/gcc8.2/bin/aarch64-linux-gnu-

git clone -b master https://gitlab.com/SakilMondal/aarch64-linux-gnu gcc8.2

rm -rf out
mkdir out
make O=out zc550kl-custom_defconfig
make O=out -j$(nproc --all)

