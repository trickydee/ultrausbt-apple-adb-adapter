#!/usr/bin/env bash
# Build A/B UF2s (release + debug) that differ only in ADB_ATTENTION_LO_MIN_US.
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

# Bump patch version for traceability on every build run.
./scripts/bump_patch_version.sh
echo ""

BOARD="${PICO_BOARD:-pico2_w}"
FIRMWARE_SRC=src/firmware
# UF2 paths are relative to each CMake build directory.
UF2_REL=src/HIDHopper-firmware.uf2
UF2_DBG=src/HIDHopper-firmware-debug.uf2
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
DIST=dist
mkdir -p "$DIST"

echo "=== A/B attention floor builds (board=$BOARD) ==="
echo "  A: ADB_ATTENTION_LO_MIN_US=500 (default)"
echo "  B: ADB_ATTENTION_LO_MIN_US=450"
echo "  For each variant: release + debug"
echo ""

BUILD_A="build-${BOARD}-ab-attn500"
BUILD_B="build-${BOARD}-ab-attn450"
BUILD_A_DBG="${BUILD_A}-debug"
BUILD_B_DBG="${BUILD_B}-debug"

cmake -B "$BUILD_A" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_ATTENTION_LO_MIN_US=500
( cd "$BUILD_A" && make -j"$CORES" )
cmake -B "$BUILD_A_DBG" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_ATTENTION_LO_MIN_US=500 -DADB_DEBUG=ON
( cd "$BUILD_A_DBG" && make -j"$CORES" )
[ -f "$BUILD_A_DBG/$UF2_REL" ] && cp "$BUILD_A_DBG/$UF2_REL" "$BUILD_A_DBG/$UF2_DBG"

cmake -B "$BUILD_B" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_ATTENTION_LO_MIN_US=450
( cd "$BUILD_B" && make -j"$CORES" )
cmake -B "$BUILD_B_DBG" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_ATTENTION_LO_MIN_US=450 -DADB_DEBUG=ON
( cd "$BUILD_B_DBG" && make -j"$CORES" )
[ -f "$BUILD_B_DBG/$UF2_REL" ] && cp "$BUILD_B_DBG/$UF2_REL" "$BUILD_B_DBG/$UF2_DBG"

OUT_A="$DIST/HIDHopper-firmware-${BOARD}-attn500.uf2"
OUT_B="$DIST/HIDHopper-firmware-${BOARD}-attn450.uf2"
OUT_A_DBG="$DIST/HIDHopper-firmware-${BOARD}-attn500-debug.uf2"
OUT_B_DBG="$DIST/HIDHopper-firmware-${BOARD}-attn450-debug.uf2"
cp "$BUILD_A/$UF2_REL" "$OUT_A"
cp "$BUILD_B/$UF2_REL" "$OUT_B"
cp "$BUILD_A_DBG/$UF2_DBG" "$OUT_A_DBG"
cp "$BUILD_B_DBG/$UF2_DBG" "$OUT_B_DBG"

echo ""
echo "Done. Flash one variant at a time and compare:"
echo "  $OUT_A"
echo "  $OUT_B"
echo "  $OUT_A_DBG"
echo "  $OUT_B_DBG"
ls -lh "$OUT_A" "$OUT_B" "$OUT_A_DBG" "$OUT_B_DBG"
