#!/usr/bin/env bash
# Lightweight cleanup for local build artifacts (safe defaults).
# - Removes build-* directories and dist/*.uf2 by default.
# - Does not remove .pico-sdk/ (SDK + picotool cache) unless you ask — keeps rebuilds fast.
# - Use --dry-run to preview.
# - Use --include-deps to also remove *_deps caches inside build dirs.
# - Use --include-sdk-cache to also remove .pico-sdk/ (forces full SDK re-clone on next build).
set -euo pipefail
cd "$(dirname "$0")/.."

DRY_RUN=0
INCLUDE_DEPS=0
INCLUDE_SDK_CACHE=0

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY_RUN=1 ;;
    --include-deps) INCLUDE_DEPS=1 ;;
    --include-sdk-cache) INCLUDE_SDK_CACHE=1 ;;
    *)
      echo "Unknown option: $arg"
      echo "Usage: $0 [--dry-run] [--include-deps] [--include-sdk-cache]"
      exit 1
      ;;
  esac
done

shopt -s nullglob

to_remove=()
for d in build-*; do
  if [ -d "$d" ]; then
    to_remove+=("$d")
  fi
done
for f in dist/*.uf2; do
  if [ -f "$f" ]; then
    to_remove+=("$f")
  fi
done

if [ "$INCLUDE_SDK_CACHE" -eq 1 ] && [ -d ".pico-sdk" ]; then
  to_remove+=(".pico-sdk")
fi

if [ "${#to_remove[@]}" -eq 0 ]; then
  echo "Nothing to clean."
  exit 0
fi

echo "Cleanup targets (${#to_remove[@]}):"
for p in "${to_remove[@]}"; do
  echo "  $p"
done
echo ""

if [ "$DRY_RUN" -eq 1 ]; then
  echo "Dry run only. No files removed."
  exit 0
fi

rm -rf "${to_remove[@]}"

if [ "$INCLUDE_DEPS" -eq 1 ]; then
  # Optional extra cleanup in case build dirs remain from concurrent runs.
  for d in build-*; do
    if [ -d "$d/_deps" ]; then
      rm -rf "$d/_deps"
    fi
  done
fi

echo "Cleanup complete."
if [ "$INCLUDE_SDK_CACHE" -eq 0 ] && [ -d ".pico-sdk" ]; then
  echo "Note: .pico-sdk/ was kept (Pico SDK + picotool cache). Use --include-sdk-cache to remove it."
fi
