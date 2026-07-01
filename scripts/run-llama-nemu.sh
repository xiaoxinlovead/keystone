#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$ROOT_DIR/build64-xs"}
LLAMA_BUILD_DIR=${LLAMA_BUILD_DIR:-/tmp/llama-build2}
MODEL_LINK=${MODEL_LINK:-/tmp/model.gguf}
MODEL_OBJ=${MODEL_OBJ:-/tmp/model.o}
MODEL_PATH=${1:-/home/yangxin/model/Llama-3.2-1B-Instruct-Q4_K_M.gguf}
NEMU_BIN=${NEMU:-/home/yangxin/xs-env/NEMU-2026.03.r3/build/riscv64-nemu-interpreter}
JOBS=${JOBS:-$(nproc)}

if [[ ! -f "$MODEL_PATH" ]]; then
  echo "error: model not found: $MODEL_PATH" >&2
  exit 1
fi

if [[ ! -x "$NEMU_BIN" ]]; then
  echo "error: NEMU binary not found: $NEMU_BIN" >&2
  exit 1
fi

source "$ROOT_DIR/source.sh"
export RISCV_ROOT_PATH="$RISCV"

prepare_llama_libs() {
  rm -rf "$LLAMA_BUILD_DIR"
  cmake -S "$ROOT_DIR/sdk/examples/llama.cpp" -B "$LLAMA_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT_DIR/sdk/examples/llama.cpp/cmake/riscv64-spacemit-linux-gnu-gcc.cmake" \
    -DGGML_CCACHE=OFF \
    -DGGML_NATIVE=OFF \
    -DGGML_OPENMP=OFF \
    -DLLAMA_BUILD_APP=OFF \
    -DLLAMA_BUILD_COMMON=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_SERVER=OFF \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_TOOLS=OFF \
    -DLLAMA_BUILD_UI=OFF

  cmake --build "$LLAMA_BUILD_DIR" -j"$JOBS" --target llama ggml ggml-base ggml-cpu
}

prepare_model_object() {
  ln -sf "$MODEL_PATH" "$MODEL_LINK"
  "$RISCV/bin/riscv64-unknown-linux-gnu-objcopy" \
    -I binary \
    -O elf64-littleriscv \
    -B riscv \
    "$MODEL_LINK" \
    "$MODEL_OBJ"
}

configure_keystone() {
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DXIANGSHAN=ON
}

ensure_initramfs_sysroot() {
  local sysroot="$BUILD_DIR/initramfs-sysroot"
  local rootfs_tar="$BUILD_DIR/buildroot.build/images/rootfs.tar"

  if [[ -f "$sysroot/.extracted" ]]; then
    return
  fi

  if [[ ! -f "$rootfs_tar" ]]; then
    cmake --build "$BUILD_DIR" -j"$JOBS" --target buildroot
  fi

  rm -rf "$sysroot"
  mkdir -p "$sysroot"
  tar -xpf "$rootfs_tar" -C "$sysroot" --exclude ./dev --exclude ./usr/share/locale
  chmod u-s "$sysroot/bin/busybox" 2>/dev/null || true
  sh "$ROOT_DIR/scripts/fix-inittab.sh" "$sysroot/etc/inittab"
  sed -i '/^::sysinit:\/etc\/init.d\/rcS$/i ::sysinit:\/bin\/mount -t devtmpfs devtmpfs \/dev' \
    "$sysroot/etc/inittab"
  cp "$ROOT_DIR/conf/S90keystone" "$sysroot/etc/init.d/S90keystone"
  chmod +x "$sysroot/etc/init.d/S90keystone"
  touch "$sysroot/.extracted"
}

refresh_initramfs_payload() {
  local sysroot="$BUILD_DIR/initramfs-sysroot"
  local overlay_keystone="$BUILD_DIR/overlay/root/keystone"
  local sysroot_keystone="$sysroot/root/keystone"

  mkdir -p "$sysroot_keystone"
  cp -af "$overlay_keystone/." "$sysroot_keystone/"
  cp "$ROOT_DIR/conf/S90keystone" "$sysroot/etc/init.d/S90keystone"
  chmod +x "$sysroot/etc/init.d/S90keystone"

  rm -f "$BUILD_DIR/linux.build/usr/initramfs_data.cpio" \
        "$BUILD_DIR/linux.build/arch/riscv/boot/Image"
}

run_nemu() {
  timeout 600 "$NEMU_BIN" -b -I 30000000000 \
    "$BUILD_DIR/sm.build/platform/nemu_xiangshan/firmware/fw_payload.bin"
}

prepare_llama_libs
prepare_model_object
configure_keystone
cmake --build "$BUILD_DIR" -j"$JOBS" --target llama_keystone-pkg
cmake --build "$BUILD_DIR" -j"$JOBS" --target image-deps
ensure_initramfs_sysroot
refresh_initramfs_payload
cmake --build "$BUILD_DIR" -j"$JOBS" --target linux
cmake --build "$BUILD_DIR" -j"$JOBS" --target sm
run_nemu
