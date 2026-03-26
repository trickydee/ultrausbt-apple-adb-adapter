#!/usr/bin/env bash
# Build HIDHopper ADB firmware for all supported boards.
# Outputs: build-<board> (release) and build-<board>-debug (ADB debug) for each board.
# Requires: PICO_SDK_PATH set, or PICO_SDK_FETCH_FROM_GIT=ON

set -e
cd "$(dirname "$0")"

echo "=== HIDHopper ADB Build (All Boards) ==="
echo ""

if [ ! -f "src/firmware/CMakeLists.txt" ]; then
    echo "Error: src/firmware/CMakeLists.txt not found. Run this script from the project root."
    exit 1
fi

# Ensure Pico SDK is available: use existing env, else find SDK in common paths, else fetch from git
if [ -z "$PICO_SDK_PATH" ] && [ -z "$PICO_SDK_FETCH_FROM_GIT" ]; then
    for candidate in "$HOME/pico/pico-sdk" "$HOME/pico-sdk" "/opt/pico-sdk" "/usr/local/pico-sdk"; do
        if [ -f "${candidate}/pico_sdk_init.cmake" ] 2>/dev/null; then
            export PICO_SDK_PATH="$candidate"
            echo "Using Pico SDK at: $PICO_SDK_PATH"
            break
        fi
    done
    if [ -z "$PICO_SDK_PATH" ]; then
        export PICO_SDK_FETCH_FROM_GIT=ON
        echo "Pico SDK not found in common paths; fetching from git (PICO_SDK_FETCH_FROM_GIT=ON)."
    fi
fi
if [ -z "$PICO_SDK_PATH" ] && [ -z "$PICO_SDK_FETCH_FROM_GIT" ]; then
    echo "Error: Could not set PICO_SDK_PATH or PICO_SDK_FETCH_FROM_GIT."
    exit 1
fi

echo "Step 1: Initializing git submodules..."
if [ -f .gitmodules ] && [ -d .git ]; then
    git submodule update --init --recursive 2>/dev/null || true
fi
echo ""

CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
FIRMWARE_SRC=src/firmware
UF2=src/HIDHopper-firmware.uf2
UF2_DEBUG=src/HIDHopper-firmware-debug.uf2
DIST_DIR=dist

mkdir -p "$DIST_DIR"
# Remove stale artifacts so dist always mirrors latest build.
rm -f "$DIST_DIR"/HIDHopper-firmware-*.uf2

# Build release firmware for a board (no ADB debug).
build_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3

    echo "=== Building for $PLATFORM_NAME ($BOARD) [release] ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME (release)..."
    # IIgs policy: suppress mouse SRQ extension to avoid BASIC slowdown when the mouse moves.
    cmake -B "$BUILD_DIR" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_IIGS_MOUSE_SUPPRESS_SRQ=ON
    echo ""

    echo "Building $PLATFORM_NAME (release)..."
    ( cd "$BUILD_DIR" && make -j"$CORES" )
    echo ""
}

# Build debug firmware for a board (ADB_DEBUG=ON, output also as HIDHopper-firmware-debug.uf2).
build_debug_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3
    local DEBUG_BUILD_DIR="${BUILD_DIR}-debug"

    echo "=== Building for $PLATFORM_NAME ($BOARD) [debug] ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME (debug, ADB_DEBUG=ON)..."
    # Keep debug enabled, but also apply IIgs policy for mouse SRQ suppression.
    cmake -B "$DEBUG_BUILD_DIR" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD" -DADB_DEBUG=ON -DADB_IIGS_MOUSE_SUPPRESS_SRQ=ON
    echo ""

    echo "Building $PLATFORM_NAME (debug)..."
    ( cd "$DEBUG_BUILD_DIR" && make -j"$CORES" )
    echo ""

    if [ -f "$DEBUG_BUILD_DIR/$UF2" ]; then
        cp "$DEBUG_BUILD_DIR/$UF2" "$DEBUG_BUILD_DIR/$UF2_DEBUG"
        echo "  Created $DEBUG_BUILD_DIR/$UF2_DEBUG"
        echo ""
    fi
}

echo "Step 2: Building for Pico (RP2040)..."
build_for_board "pico" "build-pico" "Pico (RP2040)"
build_debug_for_board "pico" "build-pico" "Pico (RP2040)"

echo "Step 3: Building for Pico W (RP2040 with CYW43)..."
build_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"
build_debug_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"

echo "Step 4: Building for Pico 2 (RP2350)..."
build_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"
build_debug_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"

echo "Step 5: Building for Pico 2 W (RP2350 with CYW43)..."
build_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"
build_debug_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"

echo "=== Build Summary ==="
echo ""

BUILD_SUCCESS=true

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
    if [ -f "$dir/$UF2_DEBUG" ]; then
        echo "✓ $name debug build successful"
        echo "  Output: $dir/$UF2_DEBUG"
        ls -lh "$dir/$UF2_DEBUG" | awk '{print "  Size: " $5}'
    else
        echo "✗ $name debug build failed (UF2 not found at $dir/$UF2_DEBUG)"
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

check_uf2 "build-pico"      "Pico (RP2040)"
check_uf2_debug "build-pico-debug" "Pico (RP2040)"
check_uf2 "build-pico_w"    "Pico W (RP2040 with CYW43)"
check_uf2_debug "build-pico_w-debug" "Pico W (RP2040 with CYW43)"
check_uf2 "build-pico2"     "Pico 2 (RP2350)"
check_uf2_debug "build-pico2-debug" "Pico 2 (RP2350)"
check_uf2 "build-pico2_w"   "Pico 2 W (RP2350 with CYW43)"
check_uf2_debug "build-pico2_w-debug" "Pico 2 W (RP2350 with CYW43)"

echo "Collecting UF2 artifacts into $DIST_DIR/ ..."
collect_dist "build-pico/$UF2" "HIDHopper-firmware-pico.uf2"
collect_dist "build-pico-debug/$UF2_DEBUG" "HIDHopper-firmware-pico-debug.uf2"
collect_dist "build-pico_w/$UF2" "HIDHopper-firmware-pico_w.uf2"
collect_dist "build-pico_w-debug/$UF2_DEBUG" "HIDHopper-firmware-pico_w-debug.uf2"
collect_dist "build-pico2/$UF2" "HIDHopper-firmware-pico2.uf2"
collect_dist "build-pico2-debug/$UF2_DEBUG" "HIDHopper-firmware-pico2-debug.uf2"
collect_dist "build-pico2_w/$UF2" "HIDHopper-firmware-pico2_w.uf2"
collect_dist "build-pico2_w-debug/$UF2_DEBUG" "HIDHopper-firmware-pico2_w-debug.uf2"
echo ""

echo "To flash: hold BOOTSEL, connect USB, then copy the .uf2 to the mounted volume:"
echo "  Release:"
echo "    - Pico:       build-pico/src/HIDHopper-firmware.uf2"
echo "    - Pico W:     build-pico_w/src/HIDHopper-firmware.uf2"
echo "    - Pico 2:     build-pico2/src/HIDHopper-firmware.uf2"
echo "    - Pico 2 W:   build-pico2_w/src/HIDHopper-firmware.uf2"
echo "  Debug (ADB serial output on UART):"
echo "    - Pico:       build-pico-debug/src/HIDHopper-firmware-debug.uf2"
echo "    - Pico W:     build-pico_w-debug/src/HIDHopper-firmware-debug.uf2"
echo "    - Pico 2:     build-pico2-debug/src/HIDHopper-firmware-debug.uf2"
echo "    - Pico 2 W:   build-pico2_w-debug/src/HIDHopper-firmware-debug.uf2"
echo "  Dist bundle:"
echo "    - dist/HIDHopper-firmware-<board>.uf2"
echo "    - dist/HIDHopper-firmware-<board>-debug.uf2"
echo ""

if [ "$BUILD_SUCCESS" = false ]; then
    exit 1
fi
