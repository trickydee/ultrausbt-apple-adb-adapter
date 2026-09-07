#!/usr/bin/env bash
# Shared helpers for firmware build scripts.
set -euo pipefail

# Keep in sync with default in src/firmware/pico_sdk_import.cmake when PICO_SDK_FETCH_FROM_GIT_TAG is unset.
PICO_SDK_TAG="${PICO_SDK_TAG:-2.2.0}"

project_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/../.."
}

firmware_repo_root() {
  # Directory containing src/firmware/CMakeLists.txt (project root when scripts live in scripts/lib/).
  (cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
}

ensure_firmware_root() {
  if [ ! -f "src/firmware/CMakeLists.txt" ]; then
    echo "Error: src/firmware/CMakeLists.txt not found. Run from project root."
    exit 1
  fi
}

ensure_pico_sdk() {
  local root
  root="$(firmware_repo_root)"
  local cache="$root/.pico-sdk"
  local cached_sdk="$cache/pico-sdk"

  # Shared picotool download/build directory for all CMake build trees (see pico-sdk tools/Findpicotool.cmake).
  export PICOTOOL_FETCH_FROM_GIT_PATH="${PICOTOOL_FETCH_FROM_GIT_PATH:-$cache}"

  # 1) Explicit PICO_SDK_PATH from environment (user override).
  if [ -n "${PICO_SDK_PATH:-}" ]; then
    if [ -f "${PICO_SDK_PATH}/pico_sdk_init.cmake" ]; then
      echo "Using Pico SDK at: $PICO_SDK_PATH (from environment)"
      return 0
    fi
    echo "Warning: PICO_SDK_PATH is set but does not look like pico-sdk; ignoring: ${PICO_SDK_PATH}"
    unset PICO_SDK_PATH
  fi

  # 2) Typical manual installs.
  if [ -z "${PICO_SDK_PATH:-}" ]; then
    local candidate
    for candidate in "$HOME/pico/pico-sdk" "$HOME/pico-sdk" "/opt/pico-sdk" "/usr/local/pico-sdk"; do
      if [ -f "${candidate}/pico_sdk_init.cmake" ] 2>/dev/null; then
        export PICO_SDK_PATH="$candidate"
        echo "Using Pico SDK at: $PICO_SDK_PATH"
        return 0
      fi
    done
  fi

  # 3) Repo-local cache: one clone for all build trees (build/, build-debug/, build-adbmon/, …).
  if [ -f "$cached_sdk/pico_sdk_init.cmake" ]; then
    export PICO_SDK_PATH="$cached_sdk"
    echo "Using Pico SDK at: $PICO_SDK_PATH (repo cache: $cache)"
    return 0
  fi

  echo "Pico SDK not found; cloning tag $PICO_SDK_TAG into $cached_sdk (one-time cache for this repo)."
  mkdir -p "$cache"
  local tmp="${cached_sdk}.tmp.$$"
  rm -rf "$tmp"
  if ! git clone --branch "$PICO_SDK_TAG" --recursive https://github.com/raspberrypi/pico-sdk.git "$tmp"; then
    rm -rf "$tmp"
    echo "Error: failed to clone pico-sdk. Set PICO_SDK_PATH to an existing SDK checkout or fix network access."
    exit 1
  fi
  rm -rf "$cached_sdk"
  mv "$tmp" "$cached_sdk"

  export PICO_SDK_PATH="$cached_sdk"
  echo "Using Pico SDK at: $PICO_SDK_PATH (repo cache: $cache)"
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
  echo "src/ultrausbt-Apple-ADB-adapter-firmware.uf2"
}

cmake_build_dir() {
  local build_dir=$1
  shift
  cmake -B "$build_dir" -S "src/firmware" "$@"
  cmake --build "$build_dir" -j"$(host_cores)"
}

adbmon_uf2_rel() {
  echo "adbmon.uf2"
}

cmake_build_adbmon_dir() {
  local build_dir=$1
  shift
  if [ ! -f "src/adbmon/CMakeLists.txt" ]; then
    echo "Error: src/adbmon/CMakeLists.txt not found."
    exit 1
  fi
  cmake -B "$build_dir" -S "src/adbmon" "$@"
  cmake --build "$build_dir" -j"$(host_cores)"
}
