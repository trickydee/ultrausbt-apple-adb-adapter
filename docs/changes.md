# HIDHopper ADB – Changes

## Firmware version

The firmware version is defined in one place and used in the serial/boot banner and anywhere the firmware identifies itself (e.g. ADB “version” response).

- **Where to set it:** `src/firmware/CMakeLists.txt` — use `project(HIDHopper-firmware VERSION x.y.z)`. Use semantic-style versions (e.g. `1.0.0`); bump when you release or tag.
- **Where it appears:** `PLATFORM_FW_VER_STRING` in the QuokkADB `platform_config.h` (product name “HIDHopper ADB” and version from CMake), printed at boot and in response to version queries.

To bump the version: edit the `VERSION` in that `project()` line and rebuild.

## SDK and TinyUSB versions

- **Pico SDK:** 2.2.0 (latest stable). When using fetch-from-git (`PICO_SDK_FETCH_FROM_GIT=ON`), the default tag is `2.2.0`. Override with `PICO_SDK_FETCH_FROM_GIT_TAG` (e.g. `master` or `2.1.1`).
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
| Pico     | `build-pico`     | `build-pico/src/HIDHopper-firmware.uf2` |
| Pico W   | `build-pico_w`   | `build-pico_w/src/HIDHopper-firmware.uf2` |
| Pico 2   | `build-pico2`    | `build-pico2/src/HIDHopper-firmware.uf2` |
| Pico 2 W | `build-pico2_w`  | `build-pico2_w/src/HIDHopper-firmware.uf2` |

Requires `PICO_SDK_PATH` or `PICO_SDK_FETCH_FROM_GIT=ON` (same as `build.sh`). Submodules are initialized automatically.
