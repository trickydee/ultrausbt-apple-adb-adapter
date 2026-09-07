#!/usr/bin/env bash
# Increment patch version in src/firmware/CMakeLists.txt:
#   project(ultrausbt-Apple-ADB-adapter-firmware VERSION X.Y.Z) -> X.Y.(Z+1)
set -euo pipefail
cd "$(dirname "$0")/.."

cmake_file="src/firmware/CMakeLists.txt"
if [ ! -f "$cmake_file" ]; then
  echo "Error: $cmake_file not found."
  exit 1
fi

current="$(sed -nE 's/.*project\(ultrausbt-Apple-ADB-adapter-firmware VERSION ([0-9]+)\.([0-9]+)\.([0-9]+)\).*/\1.\2.\3/p' "$cmake_file" | head -n1)"
if [ -z "$current" ]; then
  echo "Error: could not parse current version from $cmake_file"
  exit 1
fi

major="${current%%.*}"
rest="${current#*.}"
minor="${rest%%.*}"
patch="${current##*.}"
next_patch=$((patch + 1))
next="${major}.${minor}.${next_patch}"

sed -i.bak -E "s/project\\(ultrausbt-Apple-ADB-adapter-firmware VERSION [0-9]+\\.[0-9]+\\.[0-9]+\\)/project(ultrausbt-Apple-ADB-adapter-firmware VERSION ${next})/" "$cmake_file"
rm -f "${cmake_file}.bak"

echo "Version bumped: ${current} -> ${next}"
