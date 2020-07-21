#!/bin/bash

if [ -z "$1" ]; then
    echo "Emscripten SDK PATH is not provided"
    echo "Usage: wasm_build EMSDK_PATH"
    exit 1;
fi

if [ ! -d "./builddir_wasm" ]; then
    sed "s|EMSDK:|$1|g" wasm_cross.txt > /tmp/.wasm_cross.txt
    meson -Dthread=false -Dmodule=false -Dcache=false -Dexample=false -Db_lto=true -Ddefault_library=static builddir_wasm --cross-file /tmp/.wasm_cross.txt
    cp ./test/wasm_test.html builddir_wasm/src/index.html
fi

sudo ninja -C builddir_wasm/
echo "RESULT:"
echo " rlottie-wasm.wasm and rlottie-wasm.js can be found in builddir_wasm/src folder"
ls -lrt builddir_wasm/src/rlottie-wasm.*
