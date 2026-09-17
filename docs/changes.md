# Build notes (version, SDK, TinyUSB)

Changelog: [`release-notes.md`](release-notes.md). CMake flags: [`build-flags.md`](build-flags.md). Board pins: [`hardware.md`](hardware.md).

## Firmware version

Defined in one place and used in the boot banner / OLED:

- **Set:** `src/firmware/CMakeLists.txt` — `project(ultrausbt-Apple-ADB-adapter-firmware VERSION x.y.z)`
- **Bump:** `./scripts/bump_patch_version.sh` (or edit `VERSION` by hand) then rebuild
- **Appears as:** `PLATFORM_FW_VER_STRING` / `PRODUCT_NAME` in `platform_config.h` (`ultrausbt-Apple-ADB-adapter`)

## Pico SDK and TinyUSB

- **Pico SDK:** default tag **2.2.0** via `scripts/lib/build_common.sh` → `.pico-sdk/pico-sdk` when `PICO_SDK_PATH` is unset (`PICO_SDK_TAG` override supported).
- **TinyUSB:** SDK-bundled by default. Optional upstream submodule at `src/firmware/tinyusb` — if present, CMake sets `PICO_TINYUSB_PATH` (see `src/firmware/CMakeLists.txt`). Init with `git submodule update --init --recursive` or let `./build.sh` do it.

## Build outputs

`./build-all.sh` → `dist/`:

| Artifact | Notes |
|----------|--------|
| `ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2` | Release (ADB Device + ADB Host) |
| `…-host-debug.uf2` | UART debug @ 115200 on GP0 |
| `adbmon-pico.uf2` | Passive bus monitor |

## Bluetooth pairing (developer)

User guide: [`bluetooth-pairing.md`](bluetooth-pairing.md). Historical port notes: [`archive/`](archive/).
