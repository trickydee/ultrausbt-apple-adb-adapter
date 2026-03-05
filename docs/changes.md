# HIDHopper ADB – Changes

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
