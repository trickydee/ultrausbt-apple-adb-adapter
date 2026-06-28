#!/usr/bin/env bash
# Build BT-USB-ADB-Adapter firmware for all supported boards.
# Outputs per board: device, host, device-debug, and host-debug (ADB_DEBUG UART on GP0).
#
# Optional adbmon (passive ADB bus monitor): on by default for boards in ADBMON_BOARDS.
#   BUILD_ADBMON=0 ./build_all.sh     — adapter only (skip adbmon)
#   ./build_all.sh --no-adbmon        — same
#   ./build_all.sh --adbmon-only      — adbmon only (no adapter firmware)
#   ADBMON_BOARDS="pico" ./build_all.sh --adbmon-only  — adbmon for one board
set -euo pipefail
cd "$(dirname "$0")"
source "./scripts/lib/build_common.sh"

BUILD_ADAPTER="${BUILD_ADAPTER:-1}"
BUILD_ADBMON="${BUILD_ADBMON:-1}"
ADBMON_BOARDS="${ADBMON_BOARDS:-pico pico2_w}"
ADBMON_UF2="$(adbmon_uf2_rel)"

usage() {
    cat <<'EOF'
Usage: ./build_all.sh [options]

Builds BT-USB-ADB-Adapter firmware (device + host + debug variants) for all
supported boards, and optionally adbmon (passive ADB bus monitor).

Options:
  --no-adbmon     Build adapter firmware only (skip adbmon)
  --adbmon-only   Build adbmon only (skip adapter firmware)
  -h, --help      Show this help

Environment:
  BUILD_ADBMON=0|1      Toggle adbmon when building adapter (default: 1)
  BUILD_ADAPTER=0|1     Toggle adapter when building adbmon (default: 1)
  ADBMON_BOARDS="..."   Space-separated PICO_BOARD ids for adbmon (default: pico pico2_w)

Examples:
  ./build_all.sh
  BUILD_ADBMON=0 ./build_all.sh
  ./build_all.sh --adbmon-only
  ./build_all.sh --adbmon-only ADBMON_BOARDS="pico"   # wrong — use env:
  ADBMON_BOARDS="pico" ./build_all.sh --adbmon-only
EOF
}

for arg in "$@"; do
    case "$arg" in
        --no-adbmon)
            BUILD_ADBMON=0
            ;;
        --adbmon-only)
            BUILD_ADAPTER=0
            BUILD_ADBMON=1
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

if [ "$BUILD_ADAPTER" = "0" ] && [ "$BUILD_ADBMON" = "0" ]; then
    echo "Error: nothing to build (adapter and adbmon both disabled)." >&2
    exit 1
fi

if [ "$BUILD_ADAPTER" = "1" ]; then
    echo "=== BT-USB-ADB-Adapter Build (All Boards) ==="
else
    echo "=== adbmon Build ==="
fi
if [ "$BUILD_ADBMON" = "1" ]; then
    echo "adbmon: enabled (boards: $ADBMON_BOARDS)"
else
    echo "adbmon: skipped"
fi
if [ "$BUILD_ADAPTER" = "1" ]; then
    echo "adapter: enabled (all boards, device + host + debug)"
else
    echo "adapter: skipped"
fi
echo ""

ensure_pico_sdk
if [ "$BUILD_ADAPTER" = "1" ]; then
    ensure_firmware_root
    echo "Step 1: Initializing git submodules..."
    init_submodules
fi

UF2="$(build_dir_uf2_rel)"
UF2_DEBUG=src/BT-USB-ADB-Adapter-firmware-debug.uf2
DIST_DIR=dist
BUILD_SUCCESS=true

mkdir -p "$DIST_DIR"
# Do not wipe dist/ up front — a late build failure would leave dist empty even when
# earlier targets succeeded. collect_dist overwrites files as they are produced.

# Build release firmware for a board (ADB device mode — USB/BT → vintage Mac).
build_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3

    echo "=== Building for $PLATFORM_NAME ($BOARD) [release, device] ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME (release, ADB_HOST_MODE=OFF)..."
    cmake_build_dir "$BUILD_DIR" -DPICO_BOARD="$BOARD" -DADB_HOST_MODE=OFF || BUILD_SUCCESS=false
    echo ""
}

# Build release firmware with ADB host mode (manual switch: ADB accessories → PC USB HID).
build_host_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3
    local HOST_BUILD_DIR="${BUILD_DIR}-host"

    echo "=== Building for $PLATFORM_NAME ($BOARD) [release, host] ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME (release, ADB_HOST_MODE=ON)..."
    cmake_build_dir "$HOST_BUILD_DIR" -DPICO_BOARD="$BOARD" -DADB_HOST_MODE=ON || BUILD_SUCCESS=false
    echo ""
}

# Build debug firmware for a board (ADB_DEBUG=ON, output also as BT-USB-ADB-Adapter-firmware-debug.uf2).
build_debug_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3
    local DEBUG_BUILD_DIR="${BUILD_DIR}-debug"

    echo "=== Building for $PLATFORM_NAME ($BOARD) [debug] ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME (debug, ADB_DEBUG=ON)..."
    # ADB_DEBUG=ON; mouse SRQ suppression remains CMake default ON.
    cmake_build_dir "$DEBUG_BUILD_DIR" -DPICO_BOARD="$BOARD" -DADB_DEBUG=ON -DADB_HOST_MODE=OFF || BUILD_SUCCESS=false
    echo ""

    if [ -f "$DEBUG_BUILD_DIR/$UF2" ]; then
        cp "$DEBUG_BUILD_DIR/$UF2" "$DEBUG_BUILD_DIR/$UF2_DEBUG"
        echo "  Created $DEBUG_BUILD_DIR/$UF2_DEBUG"
        echo ""
    fi
}

# Host mode + ADB UART debug (GP0 / Pico pin 1 @ 115200): bus scan, Talk RX, status summaries.
build_host_debug_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3
    local HOST_DEBUG_BUILD_DIR="${BUILD_DIR}-host-debug"
    local UF2_HOST_DEBUG=src/BT-USB-ADB-Adapter-firmware-host-debug.uf2

    echo "=== Building for $PLATFORM_NAME ($BOARD) [host + UART debug] ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME (ADB_HOST_MODE=ON, ADB_DEBUG=ON)..."
    cmake_build_dir "$HOST_DEBUG_BUILD_DIR" -DPICO_BOARD="$BOARD" -DADB_HOST_MODE=ON -DADB_DEBUG=ON || BUILD_SUCCESS=false
    echo ""

    if [ -f "$HOST_DEBUG_BUILD_DIR/$UF2" ]; then
        cp "$HOST_DEBUG_BUILD_DIR/$UF2" "$HOST_DEBUG_BUILD_DIR/$UF2_HOST_DEBUG"
        echo "  Created $HOST_DEBUG_BUILD_DIR/$UF2_HOST_DEBUG"
        echo ""
    fi
}

build_adbmon_for_board() {
    local BOARD=$1
    local BUILD_DIR="build-adbmon-${BOARD}"

    echo "=== Building adbmon ($BOARD) [passive bus monitor] ==="
    echo ""
    cmake_build_adbmon_dir "$BUILD_DIR" -DPICO_BOARD="$BOARD" || BUILD_SUCCESS=false
    echo ""
}

if [ "$BUILD_ADAPTER" = "1" ]; then
echo "Step 2: Building for Pico (RP2040)..."
build_for_board "pico" "build-pico" "Pico (RP2040)"
build_host_for_board "pico" "build-pico" "Pico (RP2040)"
build_debug_for_board "pico" "build-pico" "Pico (RP2040)"
build_host_debug_for_board "pico" "build-pico" "Pico (RP2040)"

echo "Step 3: Building for Pico W (RP2040 with CYW43)..."
build_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"
build_host_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"
build_debug_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"
build_host_debug_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"

echo "Step 4: Building for Pico 2 (RP2350)..."
build_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"
build_host_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"
build_debug_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"
build_host_debug_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"

echo "Step 5: Building for Pico 2 W (RP2350 with CYW43)..."
build_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"
build_host_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"
build_debug_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"
build_host_debug_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"
fi

if [ "$BUILD_ADBMON" = "1" ]; then
    if [ "$BUILD_ADAPTER" = "1" ]; then
        echo "Step 6: Building adbmon (passive ADB bus monitor)..."
    else
        echo "Step 1: Building adbmon (passive ADB bus monitor)..."
    fi
    for adbmon_board in $ADBMON_BOARDS; do
        build_adbmon_for_board "$adbmon_board"
    done
fi

echo "=== Build Summary ==="
echo ""

check_uf2() {
    local dir=$1
    local name=$2
    if [ -f "$dir/$UF2" ]; then
        echo "✓ $name build successful"
        echo "  Output: $dir/$UF2"
        ls -lh "$dir/$UF2" | awk '{print "  Size: " $5}'
    else
        echo "✗ $name build failed (UF2 not found at $dir/$UF2)"
        BUILD_SUCCESS=false
    fi
    echo ""
}

check_uf2_debug() {
    local dir=$1
    local name=$2
    local uf2_name=${3:-$UF2_DEBUG}
    if [ -f "$dir/$uf2_name" ]; then
        echo "✓ $name build successful"
        echo "  Output: $dir/$uf2_name"
        ls -lh "$dir/$uf2_name" | awk '{print "  Size: " $5}'
    else
        echo "✗ $name build failed (UF2 not found at $dir/$uf2_name)"
        BUILD_SUCCESS=false
    fi
    echo ""
}

check_adbmon_uf2() {
    local dir=$1
    local name=$2
    if [ -f "$dir/$ADBMON_UF2" ]; then
        echo "✓ $name adbmon build successful"
        echo "  Output: $dir/$ADBMON_UF2"
        ls -lh "$dir/$ADBMON_UF2" | awk '{print "  Size: " $5}'
    else
        echo "✗ $name adbmon build failed (UF2 not found at $dir/$ADBMON_UF2)"
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

if [ "$BUILD_ADAPTER" = "1" ]; then
check_uf2 "build-pico"      "Pico (RP2040) device"
check_uf2 "build-pico-host" "Pico (RP2040) host"
check_uf2_debug "build-pico-debug" "Pico (RP2040)"
check_uf2_debug "build-pico-host-debug" "Pico (RP2040) host+UART" "src/BT-USB-ADB-Adapter-firmware-host-debug.uf2"
check_uf2 "build-pico_w"    "Pico W (RP2040 with CYW43) device"
check_uf2 "build-pico_w-host" "Pico W (RP2040 with CYW43) host"
check_uf2_debug "build-pico_w-debug" "Pico W (RP2040 with CYW43)"
check_uf2_debug "build-pico_w-host-debug" "Pico W (RP2040 with CYW43) host+UART" "src/BT-USB-ADB-Adapter-firmware-host-debug.uf2"
check_uf2 "build-pico2"     "Pico 2 (RP2350) device"
check_uf2 "build-pico2-host" "Pico 2 (RP2350) host"
check_uf2_debug "build-pico2-debug" "Pico 2 (RP2350)"
check_uf2_debug "build-pico2-host-debug" "Pico 2 (RP2350) host+UART" "src/BT-USB-ADB-Adapter-firmware-host-debug.uf2"
check_uf2 "build-pico2_w"   "Pico 2 W (RP2350 with CYW43) device"
check_uf2 "build-pico2_w-host" "Pico 2 W (RP2350 with CYW43) host"
check_uf2_debug "build-pico2_w-debug" "Pico 2 W (RP2350 with CYW43)"
check_uf2_debug "build-pico2_w-host-debug" "Pico 2 W (RP2350 with CYW43) host+UART" "src/BT-USB-ADB-Adapter-firmware-host-debug.uf2"
fi

if [ "$BUILD_ADBMON" = "1" ]; then
    for adbmon_board in $ADBMON_BOARDS; do
        check_adbmon_uf2 "build-adbmon-${adbmon_board}" "adbmon ($adbmon_board)"
    done
fi

echo "Collecting UF2 artifacts into $DIST_DIR/ ..."
if [ "$BUILD_ADAPTER" = "1" ]; then
collect_dist "build-pico/$UF2" "BT-USB-ADB-Adapter-firmware-pico.uf2"
collect_dist "build-pico-host/$UF2" "BT-USB-ADB-Adapter-firmware-pico-host.uf2"
collect_dist "build-pico-debug/$UF2_DEBUG" "BT-USB-ADB-Adapter-firmware-pico-debug.uf2"
collect_dist "build-pico-host-debug/src/BT-USB-ADB-Adapter-firmware-host-debug.uf2" "BT-USB-ADB-Adapter-firmware-pico-host-debug.uf2"
collect_dist "build-pico_w/$UF2" "BT-USB-ADB-Adapter-firmware-pico_w.uf2"
collect_dist "build-pico_w-host/$UF2" "BT-USB-ADB-Adapter-firmware-pico_w-host.uf2"
collect_dist "build-pico_w-debug/$UF2_DEBUG" "BT-USB-ADB-Adapter-firmware-pico_w-debug.uf2"
collect_dist "build-pico_w-host-debug/src/BT-USB-ADB-Adapter-firmware-host-debug.uf2" "BT-USB-ADB-Adapter-firmware-pico_w-host-debug.uf2"
collect_dist "build-pico2/$UF2" "BT-USB-ADB-Adapter-firmware-pico2.uf2"
collect_dist "build-pico2-host/$UF2" "BT-USB-ADB-Adapter-firmware-pico2-host.uf2"
collect_dist "build-pico2-debug/$UF2_DEBUG" "BT-USB-ADB-Adapter-firmware-pico2-debug.uf2"
collect_dist "build-pico2-host-debug/src/BT-USB-ADB-Adapter-firmware-host-debug.uf2" "BT-USB-ADB-Adapter-firmware-pico2-host-debug.uf2"
collect_dist "build-pico2_w/$UF2" "BT-USB-ADB-Adapter-firmware-pico2_w.uf2"
collect_dist "build-pico2_w-host/$UF2" "BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2"
collect_dist "build-pico2_w-debug/$UF2_DEBUG" "BT-USB-ADB-Adapter-firmware-pico2_w-debug.uf2"
collect_dist "build-pico2_w-host-debug/src/BT-USB-ADB-Adapter-firmware-host-debug.uf2" "BT-USB-ADB-Adapter-firmware-pico2_w-host-debug.uf2"
fi
if [ "$BUILD_ADBMON" = "1" ]; then
    for adbmon_board in $ADBMON_BOARDS; do
        collect_dist "build-adbmon-${adbmon_board}/${ADBMON_UF2}" "adbmon-${adbmon_board}.uf2"
    done
fi
echo ""

echo ""

if [ "$BUILD_ADAPTER" = "1" ]; then
echo "To flash adapter firmware: hold BOOTSEL, connect USB, then copy the .uf2 to the mounted volume:"
echo "  Device mode (USB/BT → vintage Mac, ADB_HOST_MODE=OFF):"
echo "    - Pico:       build-pico/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "    - Pico W:     build-pico_w/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "    - Pico 2:     build-pico2/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "    - Pico 2 W:   build-pico2_w/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "  Host mode (ADB → USB HID, manual OLED switch, ADB_HOST_MODE=ON):"
echo "    - Pico:       build-pico-host/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "    - Pico W:     build-pico_w-host/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "    - Pico 2:     build-pico2-host/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "    - Pico 2 W:   build-pico2_w-host/src/BT-USB-ADB-Adapter-firmware.uf2"
echo "  Host + UART debug (ADB host mode + GP0 ADB logs):"
echo "    - Pico 2 W:   build-pico2_w-host-debug/src/BT-USB-ADB-Adapter-firmware-host-debug.uf2"
echo "  Dist bundle:"
echo "    - dist/BT-USB-ADB-Adapter-firmware-<board>.uf2              (device)"
echo "    - dist/BT-USB-ADB-Adapter-firmware-<board>-host.uf2        (host)"
echo "    - dist/BT-USB-ADB-Adapter-firmware-<board>-debug.uf2       (device UART debug)"
echo "    - dist/BT-USB-ADB-Adapter-firmware-<board>-host-debug.uf2  (host UART debug)"
echo ""
fi
if [ "$BUILD_ADBMON" = "1" ]; then
    echo "To flash adbmon: hold BOOTSEL, connect USB, copy adbmon.uf2 to the mounted volume:"
    for adbmon_board in $ADBMON_BOARDS; do
        echo "    - ${adbmon_board}: build-adbmon-${adbmon_board}/${ADBMON_UF2}  (dist/adbmon-${adbmon_board}.uf2)"
    done
    echo "  Dist: dist/adbmon-<board>.uf2"
    echo ""
fi

if [ "$BUILD_SUCCESS" = false ]; then
    exit 1
fi
