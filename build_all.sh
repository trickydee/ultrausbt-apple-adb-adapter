#!/usr/bin/env bash
# Build HIDHopper ADB firmware for all supported boards.
# Outputs: build-pico, build-pico_w, build-pico2, build-pico2_w (at project root)
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

build_for_board() {
    local BOARD=$1
    local BUILD_DIR=$2
    local PLATFORM_NAME=$3

    echo "=== Building for $PLATFORM_NAME ($BOARD) ==="
    echo ""

    echo "Configuring build for $PLATFORM_NAME..."
    cmake -B "$BUILD_DIR" -S "$FIRMWARE_SRC" -DPICO_BOARD="$BOARD"
    echo ""

    echo "Building $PLATFORM_NAME..."
    ( cd "$BUILD_DIR" && make -j"$CORES" )
    echo ""
}

echo "Step 2: Building for Pico (RP2040)..."
build_for_board "pico" "build-pico" "Pico (RP2040)"

echo "Step 3: Building for Pico W (RP2040 with CYW43)..."
build_for_board "pico_w" "build-pico_w" "Pico W (RP2040 with CYW43)"

echo "Step 4: Building for Pico 2 (RP2350)..."
build_for_board "pico2" "build-pico2" "Pico 2 (RP2350)"

echo "Step 5: Building for Pico 2 W (RP2350 with CYW43)..."
build_for_board "pico2_w" "build-pico2_w" "Pico 2 W (RP2350 with CYW43)"

echo "=== Build Summary ==="
echo ""

BUILD_SUCCESS=true
UF2=src/HIDHopper-firmware.uf2

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

check_uf2 "build-pico"      "Pico (RP2040)"
check_uf2 "build-pico_w"    "Pico W (RP2040 with CYW43)"
check_uf2 "build-pico2"     "Pico 2 (RP2350)"
check_uf2 "build-pico2_w"   "Pico 2 W (RP2350 with CYW43)"

echo "To flash: hold BOOTSEL, connect USB, then copy the .uf2 to the mounted volume:"
echo "  - Pico:       build-pico/src/HIDHopper-firmware.uf2"
echo "  - Pico W:     build-pico_w/src/HIDHopper-firmware.uf2"
echo "  - Pico 2:     build-pico2/src/HIDHopper-firmware.uf2"
echo "  - Pico 2 W:   build-pico2_w/src/HIDHopper-firmware.uf2"
echo ""

if [ "$BUILD_SUCCESS" = false ]; then
    exit 1
fi
