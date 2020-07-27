#!/bin/bash

if [ -z "$1" ]; then
    echo "Sysroot PATH is not provided"
    echo "Usage: arm_build SYSROOT_PATH"
    exit 1;
fi

if [ ! -d "./builddir_wasm" ]; then
    sed "s|SYSROOT:|$1|g" arm_cross.txt > /tmp/.arm_cross.txt
    meson builddir_arm --cross-file /tmp/.arm_cross.txt
fi

sudo ninja -C builddir_arm/
