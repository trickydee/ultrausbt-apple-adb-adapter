# BT-USB-ADB-Adapter – Changes

Lineage: this tree descends from [HIDHopper ADB](HIDHopper.md) and QuokkADB.

## Firmware version

The firmware version is defined in one place and used in the serial/boot banner and anywhere the firmware identifies itself (e.g. ADB “version” response).

- **Where to set it:** `src/firmware/CMakeLists.txt` — use `project(BT-USB-ADB-Adapter-firmware VERSION x.y.z)`. Use semantic-style versions (e.g. `1.0.0`); bump when you release or tag.
- **Where it appears:** `PLATFORM_FW_VER_STRING` in the QuokkADB `platform_config.h` (product name **BT-USB-ADB-Adapter** and version from CMake), printed at boot and in response to version queries.

To bump the version: edit the `VERSION` in that `project()` line and rebuild.

## SDK and TinyUSB versions

- **Pico SDK:** 2.2.0 (latest stable). **`build_all.sh`**, **`./build.sh`**, and **`make`** use **`scripts/lib/build_common.sh`**: if **`PICO_SDK_PATH`** is not set, the SDK is cloned **once** into **`.pico-sdk/pico-sdk`** (default tag **`PICO_SDK_TAG=2.2.0`**, aligned with `pico_sdk_import.cmake`). That path is reused for every `build-pico`, `build-pico_w`, etc., instead of fetching a separate SDK per build directory. **`PICOTOOL_FETCH_FROM_GIT_PATH`** defaults to **`.pico-sdk`** so the pico-sdk **picotool** helper is built there once as well. You can still set **`PICO_SDK_PATH`** yourself to skip the clone. To remove the cache, run **`./scripts/cleanup_build_artifacts.sh --include-sdk-cache`**.
- **TinyUSB:** Supplied by the Pico SDK (submodule at `lib/tinyusb`). SDK 2.2.0 includes the TinyUSB version tested with that release (e.g. 0.18.x). No separate TinyUSB update is required.

**Upstream TinyUSB (implemented).** The project optionally uses upstream [hathach/tinyusb](https://github.com/hathach/tinyusb) when the submodule is present:

- **Submodule:** `src/firmware/tinyusb` (from https://github.com/hathach/tinyusb.git). Initialize with `git submodule update --init --recursive`; `build.sh` does this automatically.
- **CMake:** In `src/firmware/CMakeLists.txt`, if `src/firmware/tinyusb/hw/bsp/rp2040` exists, `PICO_TINYUSB_PATH` is set to `src/firmware/tinyusb` before the SDK is included, so the build uses that tree. If the submodule is not inited, the SDK’s bundled TinyUSB is used.
- Build output will show `Using upstream TinyUSB at ...` when the submodule is in use.

Configured in `src/firmware/CMakeLists.txt` and `src/firmware/pico_sdk_import.cmake`.

## GPIO configuration

### ADB pins (updated)

| Signal    | GPIO | Direction | Description           |
|----------|------|-----------|-----------------------|
| ADB out  | **18** | Output  | ADB data line to host |
| ADB in   | **19** | Input   | ADB data line from host |

Defined in `src/firmware/lib/QuokkADB/include/quokkadb_gpio.h` as `ADB_OUT_GPIO` and `ADB_IN_GPIO`.

### Other GPIOs (unchanged)

| GPIO | Name / use        | Direction | Description                    |
|------|-------------------|-----------|--------------------------------|
| 15   | LED_GPIO          | Output    | Status LED                     |
| 16   | UART_TX_GPIO      | UART TX   | Debug UART (115200 baud)       |
| 21   | ADB_PWR_GPIO      | —         | ADB power (defined, not used in init) |
| 22   | GPIO_TEST         | —         | Test pin (defined only)        |
| 23   | PICO_SMPS_MODE_PIN | —       | SMPS mode (board default)     |

Ensure hardware is wired for ADB data out on **GPIO 18** and ADB data in on **GPIO 19**.

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

## Build all boards

From the project root, `./build_all.sh` builds firmware for all four boards into separate directories:

| Board     | Build directory  | UF2 path |
|----------|------------------|----------|
| Pico     | `build-pico`     | `build-pico/src/BT-USB-ADB-Adapter-firmware.uf2` |
| Pico W   | `build-pico_w`   | `build-pico_w/src/BT-USB-ADB-Adapter-firmware.uf2` |
| Pico 2   | `build-pico2`    | `build-pico2/src/BT-USB-ADB-Adapter-firmware.uf2` |
| Pico 2 W | `build-pico2_w`  | `build-pico2_w/src/BT-USB-ADB-Adapter-firmware.uf2` |

Requires `PICO_SDK_PATH` or `PICO_SDK_FETCH_FROM_GIT=ON` (same as `build.sh`). Submodules are initialized automatically.

---

## Notes for editor / session restart (Mar 2025)

**Git remote:** `origin` was switched to **https://github.com/trickydee/ultramegausb-apple-adb.git** (new private repo). Local **master** was pushed to remote as **main**. These branches were pushed to origin with the same names: **feature/bluetooth**, **feature/improvements**, **feature/joysticks**, **feature/display**. All set to track their `origin/` counterparts.

**Build by branch:** On **feature/bluetooth**, the build script does *not* auto-enable SDK fetch; use `PICO_SDK_FETCH_FROM_GIT=ON ./build_all.sh` if `PICO_SDK_PATH` is not set. On **feature/joysticks** and **feature/display**, the script auto-fetches the SDK from git when not found in common paths.

**Pairing / display experiments (reverted):** On **feature/joysticks** we tried: (1) Reducing pairing delays from 50 ms/50 ms/200 ms to 10 ms; (2) `__not_in_flash_func` on the Core 1 pause path; (3) Optional display off via `ENABLE_DISPLAY_UPDATE` in `display_config.h`. All were reverted; current committed state has original delays and display always on. The doc **docs/pairing-timing-feature-bluetooth-vs-joysticks.md** records timing and code differences between **feature/bluetooth** and **feature/joysticks** for pairing investigation.
