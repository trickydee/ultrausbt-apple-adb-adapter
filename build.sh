#!/usr/bin/env bash
# Build BT-USB-ADB-Adapter Pico firmware from project root.
# Optional: upstream TinyUSB from submodule src/firmware/tinyusb (init with git submodule update --init --recursive).

set -euo pipefail
cd "$(dirname "$0")"
source "./scripts/lib/build_common.sh"

FIRMWARE_DIR=src/firmware
BUILD_DIR=$FIRMWARE_DIR/build

init_submodules
ensure_firmware_root
ensure_pico_sdk

mkdir -p "$BUILD_DIR"
cmake_build_dir "$BUILD_DIR"

echo ""
echo "Build complete. Outputs in $BUILD_DIR/src/"
echo "  UF2 for flashing: $BUILD_DIR/src/BT-USB-ADB-Adapter-firmware.uf2"
