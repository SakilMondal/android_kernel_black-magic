#!/bin/bash
BUILD_START=$(date +"%s")

# Colours
blue='\033[0;34m'
cyan='\033[0;36m'
yellow='\033[0;33m'
red='\033[0;31m'
nocol='\033[0m'

# Kernel details
KERNEL_NAME="BlackMagic™"
VERSION="N"
DATE=$(date +"%d-%m-%Y-%I-%M")
DEVICE="Z010D"
FINAL_ZIP=$KERNEL_NAME-$VERSION-$DATE-$DEVICE.zip
defconfig=zc550kl-custom_defconfig

# Dirs
KERNEL_DIR=/home/sakil/Desktop/BlackMagic/msm8916
ANYKERNEL_DIR=$KERNEL_DIR/AnyKernel2
KERNEL_IMG=$KERNEL_DIR/out/arch/arm64/boot/Image.gz-dtb
UPLOAD_DIR=/home/sakil/Desktop/BlackMagic

# Export
export ARCH=arm64
export CROSS_COMPILE=/home/sakil/AEX/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-gnu/bin/aarch64-linux-gnu-

# Toolchain Used: https://developer.arm.com/open-source/gnu-toolchain/gnu-a/downloads

## Functions ##

# Make kernel
  echo -e "$cyan***********************************************"
  echo -e "          Initializing defconfig          "
  echo -e "***********************************************$nocol"
rm -rf out
mkdir out
make O=out zc550kl-custom_defconfig
  echo -e "$cyan***********************************************"
  echo -e "             Building kernel          "
  echo -e "***********************************************$nocol"

make O=out -j$(nproc --all)

# Making zip
function make_zip() {
cp $KERNEL_IMG $ANYKERNEL_DIR
mkdir -p $UPLOAD_DIR
cd $ANYKERNEL_DIR
zip -r9 UPDATE-AnyKernel2.zip * -x README UPDATE-AnyKernel2.zip
mv $ANYKERNEL_DIR/UPDATE-AnyKernel2.zip $UPLOAD_DIR/$FINAL_ZIP
}

     echo -e "$cyan***********************************************"
     echo -e "     Making flashable zip        "
     echo -e "***********************************************$nocol"
     make_zip

# Clean Up
function cleanup(){
rm -rf $ANYKERNEL_DIR/Image.gz-dtb
}

options
cleanup
BUILD_END=$(date +"%s")
DIFF=$(($BUILD_END - $BUILD_START))
echo -e "$yellow Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds.$nocol"
  

