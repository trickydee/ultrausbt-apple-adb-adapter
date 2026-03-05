#!/usr/bin/env bash
# Build HIDHopper ADB Pico firmware from project root.
# Requires: Raspberry Pi Pico SDK, PICO_SDK_PATH set (or PICO_SDK_FETCH_FROM_GIT=ON).
# Optional: upstream TinyUSB from submodule src/firmware/tinyusb (init with git submodule update --init --recursive).

set -e
cd "$(dirname "$0")"

FIRMWARE_DIR=src/firmware
BUILD_DIR=$FIRMWARE_DIR/build

# Initialize submodules (e.g. upstream TinyUSB at src/firmware/tinyusb) if present
if [ -f .gitmodules ] && [ -d .git ]; then
  git submodule update --init --recursive 2>/dev/null || true
fi

if [ -z "$PICO_SDK_PATH" ] && [ -z "$PICO_SDK_FETCH_FROM_GIT" ]; then
  echo "Error: Set PICO_SDK_PATH to your pico-sdk directory, or set PICO_SDK_FETCH_FROM_GIT=ON"
  exit 1
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
make

echo ""
echo "Build complete. Outputs in $BUILD_DIR/src/"
echo "  UF2 for flashing: $BUILD_DIR/src/HIDHopper-firmware.uf2"
