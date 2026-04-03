#!/usr/bin/env bash
# Run cmake for firmware with the same Pico SDK environment as build_all.sh.
# Used by the top-level Makefile (separate process; needs shared cache setup).
set -euo pipefail
cd "$(dirname "$0")/.."
# shellcheck source=scripts/lib/build_common.sh
source "./scripts/lib/build_common.sh"
ensure_firmware_root
ensure_pico_sdk
exec cmake "$@"
