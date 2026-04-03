#!/usr/bin/env bash
# Build A/B UF2s (release + debug) that differ only in ADB_STRICT_DUTY_CYCLE_DECODE.
# Default board: pico2_w (override with PICO_BOARD=pico_w etc.)
set -euo pipefail
cd "$(dirname "$0")/.."
source "./scripts/lib/build_common.sh"

ensure_firmware_root
ensure_pico_sdk
init_submodules

# Bump patch version for traceability on every build run.
./scripts/bump_patch_version.sh
echo ""

BOARD="${PICO_BOARD:-pico2_w}"
UF2_REL="$(build_dir_uf2_rel)"
UF2_DBG=src/HIDHopper-firmware-debug.uf2
DIST=dist
mkdir -p "$DIST"

echo "=== A/B duty-cycle decode builds (board=$BOARD) ==="
echo "  A: ADB_STRICT_DUTY_CYCLE_DECODE=ON"
echo "  B: ADB_STRICT_DUTY_CYCLE_DECODE=OFF (midpoint legacy)"
echo "  For each variant: release + debug"
echo ""

BUILD_A="build-${BOARD}-ab-decode-strict-on"
BUILD_B="build-${BOARD}-ab-decode-strict-off"
BUILD_A_DBG="${BUILD_A}-debug"
BUILD_B_DBG="${BUILD_B}-debug"

cmake_build_dir "$BUILD_A" -DPICO_BOARD="$BOARD" -DADB_STRICT_DUTY_CYCLE_DECODE=ON
cmake_build_dir "$BUILD_A_DBG" -DPICO_BOARD="$BOARD" -DADB_STRICT_DUTY_CYCLE_DECODE=ON -DADB_DEBUG=ON
[ -f "$BUILD_A_DBG/$UF2_REL" ] && cp "$BUILD_A_DBG/$UF2_REL" "$BUILD_A_DBG/$UF2_DBG"

cmake_build_dir "$BUILD_B" -DPICO_BOARD="$BOARD" -DADB_STRICT_DUTY_CYCLE_DECODE=OFF
cmake_build_dir "$BUILD_B_DBG" -DPICO_BOARD="$BOARD" -DADB_STRICT_DUTY_CYCLE_DECODE=OFF -DADB_DEBUG=ON
[ -f "$BUILD_B_DBG/$UF2_REL" ] && cp "$BUILD_B_DBG/$UF2_REL" "$BUILD_B_DBG/$UF2_DBG"

OUT_A="$DIST/HIDHopper-firmware-${BOARD}-decode-strict-on.uf2"
OUT_B="$DIST/HIDHopper-firmware-${BOARD}-decode-strict-off.uf2"
OUT_A_DBG="$DIST/HIDHopper-firmware-${BOARD}-decode-strict-on-debug.uf2"
OUT_B_DBG="$DIST/HIDHopper-firmware-${BOARD}-decode-strict-off-debug.uf2"
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
