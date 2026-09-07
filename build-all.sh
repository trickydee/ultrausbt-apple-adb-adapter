#!/usr/bin/env bash
# Build ultrausbt-Apple-ADB-adapter firmware and adbmon.
#
# Produces:
#   - Unified adapter UF2 (ADB device + host modes, OLED toggle) for Pico 2 W
#   - Debug adapter UF2 (ADB UART on GP0 @ 115200)
#   - adbmon UF2 for original Pico (passive ADB bus monitor)
#
# Outputs are copied to dist/ with stable names. On success, CMake build trees
# (build/, build-debug/, build-adbmon/) are removed unless BUILD_KEEP_DIRS=1.
set -euo pipefail
cd "$(dirname "$0")"
source "./scripts/lib/build_common.sh"

ADAPTER_BOARD="${ADAPTER_BOARD:-pico2_w}"
ADBMON_BOARD="${ADBMON_BOARD:-pico}"
BUILD_ADBMON="${BUILD_ADBMON:-1}"

BUILD_RELEASE_DIR="${BUILD_RELEASE_DIR:-build}"
BUILD_DEBUG_DIR="${BUILD_DEBUG_DIR:-build-debug}"
BUILD_ADBMON_DIR="${BUILD_ADBMON_DIR:-build-adbmon}"

UF2="$(build_dir_uf2_rel)"
UF2_DEBUG=src/ultrausbt-Apple-ADB-adapter-firmware-host-debug.uf2
ADBMON_UF2="$(adbmon_uf2_rel)"
DIST_DIR=dist
BUILD_SUCCESS=true

usage() {
    cat <<'EOF'
Usage: ./build-all.sh [options]

Builds the unified ultrausbt-Apple-ADB-adapter firmware (device + host toggle) for Pico 2 W,
the UART debug build, and adbmon for Pico.

Options:
  --no-adbmon     Skip adbmon build
  -h, --help      Show this help

Environment:
  ADAPTER_BOARD=pico2_w   Pico board for adapter firmware (default: pico2_w)
  ADBMON_BOARD=pico       Pico board for adbmon (default: pico)
  BUILD_ADBMON=0|1        Skip adbmon when 0 (default: 1)
  BUILD_RELEASE_DIR       CMake build tree for release (default: build)
  BUILD_DEBUG_DIR         CMake build tree for debug (default: build-debug)
  BUILD_ADBMON_DIR        CMake build tree for adbmon (default: build-adbmon)
  BUILD_KEEP_DIRS=1       Keep CMake build trees after success (default: remove)
EOF
}

cleanup_build_dirs() {
    local dir
    for dir in "$BUILD_RELEASE_DIR" "$BUILD_DEBUG_DIR"; do
        if [ -d "$dir" ]; then
            echo "  Removing $dir/"
            rm -rf "$dir"
        fi
    done
    if [ "$BUILD_ADBMON" = "1" ] && [ -d "$BUILD_ADBMON_DIR" ]; then
        echo "  Removing $BUILD_ADBMON_DIR/"
        rm -rf "$BUILD_ADBMON_DIR"
    fi
}

for arg in "$@"; do
    case "$arg" in
        --no-adbmon)
            BUILD_ADBMON=0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            usage >&2
            exit 1
            ;;
    esac
done

echo "=== ultrausbt-Apple-ADB-adapter build ==="
echo "Adapter: $ADAPTER_BOARD (device + host toggle, ADB_HOST_MODE=ON)"
echo "Debug:   $ADAPTER_BOARD (ADB_DEBUG=ON)"
if [ "$BUILD_ADBMON" = "1" ]; then
    echo "adbmon:  $ADBMON_BOARD"
else
    echo "adbmon:  skipped"
fi
echo ""

ensure_pico_sdk
ensure_firmware_root
init_submodules

mkdir -p "$DIST_DIR"

build_adapter_release() {
    echo "=== Adapter release ($ADAPTER_BOARD) ==="
    cmake_build_dir "$BUILD_RELEASE_DIR" \
        -DPICO_BOARD="$ADAPTER_BOARD" \
        -DADB_HOST_MODE=ON || BUILD_SUCCESS=false
    echo ""
}

build_adapter_debug() {
    echo "=== Adapter debug ($ADAPTER_BOARD) ==="
    cmake_build_dir "$BUILD_DEBUG_DIR" \
        -DPICO_BOARD="$ADAPTER_BOARD" \
        -DADB_HOST_MODE=ON \
        -DADB_DEBUG=ON || BUILD_SUCCESS=false

    if [ -f "$BUILD_DEBUG_DIR/$UF2" ]; then
        cp "$BUILD_DEBUG_DIR/$UF2" "$BUILD_DEBUG_DIR/$UF2_DEBUG"
        echo "  Created $BUILD_DEBUG_DIR/$UF2_DEBUG"
    fi
    echo ""
}

build_adbmon() {
    echo "=== adbmon ($ADBMON_BOARD) ==="
    cmake_build_adbmon_dir "$BUILD_ADBMON_DIR" \
        -DPICO_BOARD="$ADBMON_BOARD" || BUILD_SUCCESS=false
    echo ""
}

build_adapter_release
build_adapter_debug
if [ "$BUILD_ADBMON" = "1" ]; then
    build_adbmon
fi

check_uf2() {
    local path=$1
    local label=$2
    if [ -f "$path" ]; then
        echo "✓ $label"
        echo "  $path"
        ls -lh "$path" | awk '{print "  Size: " $5}'
    else
        echo "✗ $label (missing: $path)"
        BUILD_SUCCESS=false
    fi
    echo ""
}

collect_dist() {
    local src_path=$1
    local dist_name=$2
    if [ -f "$src_path" ]; then
        cp "$src_path" "$DIST_DIR/$dist_name"
        echo "  Dist: $DIST_DIR/$dist_name"
    fi
}

echo "=== Build summary ==="
echo ""

RELEASE_UF2="$BUILD_RELEASE_DIR/$UF2"
DEBUG_UF2="$BUILD_DEBUG_DIR/$UF2_DEBUG"
ADBMON_OUT="$BUILD_ADBMON_DIR/$ADBMON_UF2"

check_uf2 "$RELEASE_UF2" "Adapter release"
check_uf2 "$DEBUG_UF2" "Adapter debug (UART)"
if [ "$BUILD_ADBMON" = "1" ]; then
    check_uf2 "$ADBMON_OUT" "adbmon ($ADBMON_BOARD)"
fi

echo "Collecting UF2 artifacts into $DIST_DIR/ ..."
collect_dist "$RELEASE_UF2" "ultrausbt-Apple-ADB-adapter-firmware-${ADAPTER_BOARD}-host.uf2"
collect_dist "$DEBUG_UF2" "ultrausbt-Apple-ADB-adapter-firmware-${ADAPTER_BOARD}-host-debug.uf2"
if [ "$BUILD_ADBMON" = "1" ]; then
    collect_dist "$ADBMON_OUT" "adbmon-${ADBMON_BOARD}.uf2"
fi
echo ""

echo "Flash (hold BOOTSEL, connect USB, copy UF2):"
echo "  Adapter:  $DIST_DIR/ultrausbt-Apple-ADB-adapter-firmware-${ADAPTER_BOARD}-host.uf2"
echo "  Debug:    $DIST_DIR/ultrausbt-Apple-ADB-adapter-firmware-${ADAPTER_BOARD}-host-debug.uf2"
if [ "$BUILD_ADBMON" = "1" ]; then
    echo "  adbmon:   $DIST_DIR/adbmon-${ADBMON_BOARD}.uf2"
fi
echo ""

if [ "$BUILD_SUCCESS" = false ]; then
    exit 1
fi

if [ "${BUILD_KEEP_DIRS:-0}" != "1" ]; then
    echo "Cleaning up CMake build directories (artifacts are in $DIST_DIR/) ..."
    cleanup_build_dirs
    echo ""
fi
