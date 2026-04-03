#!/usr/bin/env bash
# Shared helpers for firmware build scripts.
set -euo pipefail

project_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/../.."
}

ensure_firmware_root() {
  if [ ! -f "src/firmware/CMakeLists.txt" ]; then
    echo "Error: src/firmware/CMakeLists.txt not found. Run from project root."
    exit 1
  fi
}

ensure_pico_sdk() {
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
}

init_submodules() {
  echo "Initializing git submodules..."
  if [ -f .gitmodules ] && [ -d .git ]; then
    git submodule update --init --recursive 2>/dev/null || true
  fi
  echo ""
}

host_cores() {
  nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4
}

build_dir_uf2_rel() {
  echo "src/HIDHopper-firmware.uf2"
}

cmake_build_dir() {
  local build_dir=$1
  shift
  cmake -B "$build_dir" -S "src/firmware" "$@"
  cmake --build "$build_dir" -j"$(host_cores)"
}
