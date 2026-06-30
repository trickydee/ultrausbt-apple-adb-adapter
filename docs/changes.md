# BT-USB-ADB-Adapter – Changes

Lineage: this tree descends from QuokkADB and adbuino. Target hardware is documented in [`hardware.md`](hardware.md).

## Firmware version

The firmware version is defined in one place and used in the serial/boot banner and anywhere the firmware identifies itself (e.g. ADB “version” response).

- **Where to set it:** `src/firmware/CMakeLists.txt` — use `project(BT-USB-ADB-Adapter-firmware VERSION x.y.z)`. Use semantic-style versions (e.g. `1.0.0`); bump when you release or tag.
- **Where it appears:** `PLATFORM_FW_VER_STRING` in the QuokkADB `platform_config.h` (product name **BT-USB-ADB-Adapter** and version from CMake), printed at boot and in response to version queries.

To bump the version: edit the `VERSION` in that `project()` line and rebuild.

## SDK and TinyUSB versions

- **Pico SDK:** 2.2.0 (latest stable). **`build-all.sh`**, **`./build.sh`**, and **`make`** use **`scripts/lib/build_common.sh`**: if **`PICO_SDK_PATH`** is not set, the SDK is cloned **once** into **`.pico-sdk/pico-sdk`** (default tag **`PICO_SDK_TAG=2.2.0`**, aligned with `pico_sdk_import.cmake`). **`PICOTOOL_FETCH_FROM_GIT_PATH`** defaults to **`.pico-sdk`** so the pico-sdk **picotool** helper is built there once as well. You can still set **`PICO_SDK_PATH`** yourself to skip the clone. To remove the cache, run **`./scripts/cleanup_build_artifacts.sh --include-sdk-cache`**.
- **TinyUSB:** Supplied by the Pico SDK (submodule at `lib/tinyusb`). SDK 2.2.0 includes the TinyUSB version tested with that release (e.g. 0.18.x). No separate TinyUSB update is required.

**Upstream TinyUSB (implemented).** The project optionally uses upstream [hathach/tinyusb](https://github.com/hathach/tinyusb) when the submodule is present:

- **Submodule:** `src/firmware/tinyusb` (from https://github.com/hathach/tinyusb.git). Initialize with `git submodule update --init --recursive`; `build.sh` does this automatically.
- **CMake:** In `src/firmware/CMakeLists.txt`, if `src/firmware/tinyusb/hw/bsp/rp2040` exists, `PICO_TINYUSB_PATH` is set to `src/firmware/tinyusb` before the SDK is included, so the build uses that tree. If the submodule is not inited, the SDK’s bundled TinyUSB is used.
- Build output will show `Using upstream TinyUSB at ...` when the submodule is in use.

Configured in `src/firmware/CMakeLists.txt` and `src/firmware/pico_sdk_import.cmake`.

## GPIO configuration

### ADB DATA GPIO (device vs host)

| Mode | `data_hi()` behaviour | Where |
|------|----------------------|--------|
| **ADB → Mac** (device) | Drive GP18 **high** (`ADB_OUT_HIGH`) | `adb_platform.h` |
| **ADB → USB** (host TX) | Tri-state (open-collector release) | `adb_host_gpio.h` only |

Firmware **2.0.0** briefly used tri-state in shared `adb_platform.h`, which broke device-mode collision detection (BT mouse jumps). **2.1.0** split GPIO as above. See [`troubleshooting.md`](troubleshooting.md) and [`adb-shared-gpio-rollback.md`](adb-shared-gpio-rollback.md).

### ADB pins

| Signal    | GPIO | Direction | Description           |
|----------|------|-----------|-----------------------|
| ADB out  | **18** | Output  | Uc1 → ADB DATA (TX) |
| ADB in   | **19** | Input   | Uc1 → ADB DATA (RX) |
| Status LED | **25** | Output | D1 via R1 |

Defined in `src/firmware/lib/QuokkADB/include/quokkadb_gpio.h`. Schematic: **apple-adb.kicad_sch** — see [`hardware.md`](hardware.md).

### Other GPIOs

| GPIO | Name / use        | Direction | Description                    |
|------|-------------------|-----------|--------------------------------|
| 0    | UART_TX_GPIO      | UART TX   | Debug UART TX (Pico **pin 1**, 115200 baud) |
| 1    | UART_RX_GPIO      | UART RX   | Debug UART RX (Pico **pin 3**, optional)   |
| 2–9  | Display1 header   | —         | Optional OLED / UI (see `display_config.h`) |
| 22   | GPIO_TEST         | —         | Test pin (defined only)        |
| 23   | PICO_SMPS_MODE_PIN | —       | SMPS mode (board default)     |

### SSD1306 OLED display (optional)

An I2C SSD1306 128×64 OLED can be connected for splash, device counts, and (on Pico W / Pico 2 W) Bluetooth device names. No GPIO collision with ADB, LED, or UART.

| GPIO | Use           | Description                    |
|------|----------------|--------------------------------|
| 4    | I2C SDA        | Display data (i2c0)            |
| 5    | I2C SCL        | Display clock (i2c0)           |
| 6    | Button left    | Optional; active low           |
| 7    | Button middle  | Cycle screens (splash → devices → BT names) |
| 8    | Button right   | On splash: clear BT pairings (when BT enabled) |

Config: `src/firmware/src/display/display_config.h`. Display address 0x3c. If no display is connected, the firmware still runs; I2C init is attempted at boot.

## Bluepad32 (Bluetooth keyboard and mouse)

Bluetooth HID support is available on **Pico W** and **Pico 2 W** only (boards with CYW43). It uses [Bluepad32](https://github.com/ricardoquesada/bluepad32) for BT keyboard and mouse; gamepads are not implemented yet.

- **Submodule:** `src/firmware/bluepad32`. Initialize with `git submodule update --init --recursive` (or let `build.sh` do it).
- **Build for Bluetooth:** Use a wireless board so Bluepad32 is enabled:
  - From a clean build dir:  
    `cd src/firmware/build && cmake -DPICO_BOARD=pico_w ..` (or `pico2_w`) then `make`
  - Or with build script: set board in CMake (e.g. edit `build.sh` to pass `-DPICO_BOARD=pico_w` to `cmake`).
- **Default build** (no `-DPICO_BOARD=pico_w`): builds for **pico**; Bluepad32 is disabled and the firmware is USB-only.
- **Behaviour:** When built for `pico_w` or `pico2_w`, the firmware starts Bluetooth scanning after init. Paired BT keyboards and mice feed into the same ADB pipeline as USB (same parsers and register handling). Up to 2 BT keyboards and 2 BT mice are supported; only the first of each is currently processed in the main loop.
- **Files:** `src/firmware/src/bluepad32_init.c`, `bluepad32_platform.c`, `btstack_config.h`, `sdkconfig.h`; `lib/QuokkADB/src/bt_hid_bridge.cpp`; platform API in `bluepad32_platform.h`, app API in `bluepad32_api.h`.

## Build (`build-all.sh`)

From the project root, **`./build-all.sh`** produces three UF2s in **`dist/`**:

| Artifact | Board | CMake | Build dir |
|----------|-------|-------|-----------|
| `BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2` | Pico 2 W | `ADB_HOST_MODE=ON` | `build/` |
| `BT-USB-ADB-Adapter-firmware-pico2_w-host-debug.uf2` | Pico 2 W | `ADB_HOST_MODE=ON`, `ADB_DEBUG=ON` | `build-debug/` |
| `adbmon-pico.uf2` | Pico | adbmon project | `build-adbmon/` |

The **host** UF2 is the unified product image: **ADB → Mac** (default) and **ADB → USB** (OLED toggle). Skip adbmon with `./build-all.sh --no-adbmon`.

On **success**, CMake trees (`build/`, `build-debug/`, `build-adbmon/`) are **removed** after UF2s are copied to `dist/`. Set **`BUILD_KEEP_DIRS=1`** to keep them for incremental rebuilds.

**Quick dev build:** `./build.sh` or `make` → `src/firmware/build/src/BT-USB-ADB-Adapter-firmware.uf2` (configure board via CMake as needed).

Other boards (`pico`, `pico_w`, `pico2`) can still be built manually with `cmake -DPICO_BOARD=…`. The DIY ultramegausb board targets **Pico 2 W**.

Requires `PICO_SDK_PATH` or the repo `.pico-sdk/` cache (see `build_common.sh`). Submodules are initialized automatically.

## Bluetooth pairing stability (planned work)

Random BLE pairing hangs on Pico W / Pico 2 W are tracked in [`docs/FUTURE_WORK.md`](FUTURE_WORK.md) §1 and detailed in:

- [`docs/BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) — canonical fix recipe (copied from ultramegausb-atari-st-rpikbd v22.1.0).
- [`docs/BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) — this repo’s gap analysis, file map, test matrix, and prior experiments.

Firmware port **not started**; see companion doc for checklist vs current code.

---

## Notes for editor / session restart (Mar 2025)

**Git remote:** `origin` was switched to **https://github.com/trickydee/ultramegausb-apple-adb.git** (new private repo). Local **master** was pushed to remote as **main**. These branches were pushed to origin with the same names: **feature/bluetooth**, **feature/improvements**, **feature/joysticks**, **feature/display**. All set to track their `origin/` counterparts.

**Build by branch:** Use **`./build-all.sh`** from `main`; SDK is resolved via `build_common.sh` (`.pico-sdk/` cache or `PICO_SDK_PATH`).

**Pairing / display experiments (reverted):** On **feature/joysticks** we tried: (1) Reducing pairing delays from 50 ms/50 ms/200 ms to 10 ms; (2) `__not_in_flash_func` on the Core 1 pause path; (3) Optional display off via `ENABLE_DISPLAY_UPDATE` in `display_config.h`. All were reverted; current committed state has original delays and display always on. The doc **docs/pairing-timing-feature-bluetooth-vs-joysticks.md** records timing and code differences between **feature/bluetooth** and **feature/joysticks** for pairing investigation.
