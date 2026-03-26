#!/usr/bin/env bash
# Build two release UF2s that differ only in ADB_ATTENTION_LO_MIN_US (same PROJECT_VERSION).
# Default board: pico2_w (override with PICO_BOARD=pico_w etc.)
# Requires: PICO_SDK_PATH set, or PICO_SDK_FETCH_FROM_GIT=ON (same discovery as build_all.sh)
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -f "src/firmware/CMakeLists.txt" ]; then
  echo "Error: src/firmware/CMakeLists.txt not found. Run this script from the project repo (scripts/ is under the root)."
  exit 1
fi

# Ensure Pico SDK is available: use existing env, else find SDK in common paths, else fetch from git
if [ -z "${PICO_SDK_PATH:-}" ] && [ -z "${PICO_SDK_FETCH_FROM_GIT:-}" ]; then
  for candidate in "$HOME/pico/pico-sdk" "$HOME/pico-sdk" "/opt/pico-sdk" "/usr/local/pico-sdk"; do
    if [ -f "${candidate}/pico_sdk_init.cmake" ] 2>/dev/null; then
      export PICO_SDK_PATH="$candidate"
      echo "Using Pico SDK at: $PICO_SDK_PATH"
      break
    fi
  done
  if [ -z "${PICO_SDK_PATH:-}" ]; then
    export PICO_SDK_FETCH_FROM_GIT=ON
    echo "Pico SDK not found in common paths; fetching from git (PICO_SDK_FETCH_FROM_GIT=ON)."
  fi
fi
if [ -z "${PICO_SDK_PATH:-}" ] && [ -z "${PICO_SDK_FETCH_FROM_GIT:-}" ]; then
  echo "Error: Could not set PICO_SDK_PATH or PICO_SDK_FETCH_FROM_GIT."
  exit 1
fi

echo "Initializing git submodules..."
if [ -f .gitmodules ] && [ -d .git ]; then
  git submodule update --init --recursive 2>/dev/null || true
fi
echo ""

BOARD="${PICO_BOARD:-pico2_w}"
FIRMWARE_SRC=src/firmware
# UF2 path is relative to the CMake build directory (same as build_all.sh)
UF2_REL=src/HIDHopper-firmware.uf2
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
DIST=dist
mkdir -p "$DIST"

echo "=== A/B attention floor builds (board=$BOARD) ==="
echo "  A: ADB_ATTENTION_LO_MIN_US=500 (default)"
echo "  B: ADB_ATTENTION_LO_MIN_US=450"
echo ""

BUILD_A="build-${BOARD}-ab-attn500"
BUILD_B="build-${BOARD}-ab-attn450"

cmake -B "$BUILD_A" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_ATTENTION_LO_MIN_US=500
( cd "$BUILD_A" && make -j"$CORES" )

cmake -B "$BUILD_B" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_ATTENTION_LO_MIN_US=450
( cd "$BUILD_B" && make -j"$CORES" )

OUT_A="$DIST/HIDHopper-firmware-${BOARD}-attn500.uf2"
OUT_B="$DIST/HIDHopper-firmware-${BOARD}-attn450.uf2"
cp "$BUILD_A/$UF2_REL" "$OUT_A"
cp "$BUILD_B/$UF2_REL" "$OUT_B"

echo ""
echo "Done. Flash one, test keyboard+mouse, then the other:"
echo "  $OUT_A"
echo "  $OUT_B"
ls -lh "$OUT_A" "$OUT_B"
