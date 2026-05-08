#!/bin/bash

set -e

if [ -z "$BITS" ]; then
  BITS=64
fi

echo "Fast Setup (RV$BITS)";

DIST=xenial

if [[ $(ldconfig -p | grep "libmpfr.so.6") ]]; then
  DIST=bionic
fi


echo "Updating and cloning submodules, this may take a long time"
git config submodule.riscv-gnu-toolchain.update none

# shallow clone submodules ahead of time (Git must be > 2.11)

if [ ! -e buildroot/.git ]; then
  git clone --shallow-since=2020-04-15 https://github.com/buildroot/buildroot.git buildroot
fi


# build SDK if not present
if [ ! -z $KEYSTONE_SDK_DIR ] && [ -e $KEYSTONE_SDK_DIR ]
then
  echo "KEYSTONE_SDK_DIR is set to $KEYSTONE_SDK_DIR and present. Skipping SDK installation."
else
  echo "KEYSTONE_SDK_DIR is not set or present. Installing from $(pwd)/sdk"
  export KEYSTONE_SDK_DIR=$(pwd)/sdk/build$BITS
  cd sdk
  mkdir -p build
  cd build
  cmake .. $SDK_FLAGS
  make
  make install
  cd ../..
fi

# update source.sh
GCC_PATH=$(which riscv$BITS-unknown-linux-gnu-gcc)
RISCV_DIR=$(dirname $(dirname $GCC_PATH))
echo "export RISCV=$RISCV_DIR" > ./source.sh
echo "export PATH=\$RISCV/bin:\$PATH" >> ./source.sh
echo "export KEYSTONE_SDK_DIR=$KEYSTONE_SDK_DIR" >> ./source.sh

echo "RISC-V toolchain and Keystone SDK have been fully setup"
echo ""
echo " * Notice: run the following command to update enviroment variables *"
echo ""
echo "           source ./source.sh"
echo ""
