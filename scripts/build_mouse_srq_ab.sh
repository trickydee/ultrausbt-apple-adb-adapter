#!/usr/bin/env bash
# Build release + debug UF2s that A/B test mouse SRQ suppression:
#  - OFF: legacy behavior (kbd or mouse can request SRQ)
#  - ON : suppress ADB SRQ extension for mouse (keyboard only)
#
# The goal is to see if IIgs BASIC slowdown when the mouse is moved is caused
# by extra SRQ/service-request traffic.
#
# Default board: pico2_w (override with PICO_BOARD=pico_w etc.)
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -f "src/firmware/CMakeLists.txt" ]; then
  echo "Error: src/firmware/CMakeLists.txt not found. Run from project root."
  exit 1
fi

# Ensure pico sdk is available: use same discovery as build_all.sh
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

./scripts/bump_patch_version.sh
echo ""

BOARD="${PICO_BOARD:-pico2_w}"
FIRMWARE_SRC=src/firmware
UF2_REL=src/HIDHopper-firmware.uf2
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
DIST=dist
mkdir -p "$DIST"

echo "=== A/B mouse SRQ suppression builds (board=$BOARD) ==="
echo "  A: ADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF (kbd or mouse SRQ)"
echo "  B: ADB_IIGS_MOUSE_SUPPRESS_SRQ=ON  (keyboard only)"
echo ""

BUILD_A="build-${BOARD}-ab-mouse-srq-off"
BUILD_B="build-${BOARD}-ab-mouse-srq-on"
BUILD_A_DBG="${BUILD_A}-debug"
BUILD_B_DBG="${BUILD_B}-debug"

# Keep other tuning fixed to safe defaults for ADB decode:
# - strict duty decode OFF
# - strict sync window OFF
COMMON_CMAKE=(-DPICO_BOARD="$BOARD" -DADB_STRICT_DUTY_CYCLE_DECODE=OFF -DADB_STRICT_SYNC_WINDOW=OFF)

cmake -B "$BUILD_A" -S "$FIRMWARE_SRC" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF
( cd "$BUILD_A" && make -j"$CORES" )

cmake -B "$BUILD_B" -S "$FIRMWARE_SRC" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=ON
( cd "$BUILD_B" && make -j"$CORES" )

cmake -B "$BUILD_A_DBG" -S "$FIRMWARE_SRC" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF -DADB_DEBUG=ON
( cd "$BUILD_A_DBG" && make -j"$CORES" )

cmake -B "$BUILD_B_DBG" -S "$FIRMWARE_SRC" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=ON -DADB_DEBUG=ON
( cd "$BUILD_B_DBG" && make -j"$CORES" )

OUT_A="$DIST/HIDHopper-firmware-${BOARD}-mouse-srq-off.uf2"
OUT_B="$DIST/HIDHopper-firmware-${BOARD}-mouse-srq-on.uf2"
OUT_A_DBG="$DIST/HIDHopper-firmware-${BOARD}-mouse-srq-off-debug.uf2"
OUT_B_DBG="$DIST/HIDHopper-firmware-${BOARD}-mouse-srq-on-debug.uf2"

cp "$BUILD_A/$UF2_REL" "$OUT_A"
cp "$BUILD_B/$UF2_REL" "$OUT_B"
cp "$BUILD_A_DBG/$UF2_REL" "$OUT_A_DBG"
cp "$BUILD_B_DBG/$UF2_REL" "$OUT_B_DBG"

echo ""
echo "Done. Test one at a time:"
echo "  $OUT_A"
echo "  $OUT_B"
echo "Debug:"
echo "  $OUT_A_DBG"
echo "  $OUT_B_DBG"
ls -lh "$OUT_A" "$OUT_B" "$OUT_A_DBG" "$OUT_B_DBG"

