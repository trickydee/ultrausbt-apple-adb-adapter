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
source "./scripts/lib/build_common.sh"

ensure_firmware_root
ensure_pico_sdk
init_submodules

./scripts/bump_patch_version.sh
echo ""

BOARD="${PICO_BOARD:-pico2_w}"
UF2_REL="$(build_dir_uf2_rel)"
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

cmake_build_dir "$BUILD_A" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF

cmake_build_dir "$BUILD_B" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=ON

cmake_build_dir "$BUILD_A_DBG" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF -DADB_DEBUG=ON

cmake_build_dir "$BUILD_B_DBG" "${COMMON_CMAKE[@]}" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=ON -DADB_DEBUG=ON

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

