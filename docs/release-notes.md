# BT-USB-ADB-Adapter – Release notes

Lineage: this firmware continues the [HIDHopper ADB](HIDHopper.md) / QuokkADB line. Older notes below still name **HIDHopper** where they describe history or retail hardware.

## feature/gamepad (in progress)

- **Bluetooth gamepad:** One BT gamepad slot in Bluepad32 (`uni_gamepad_t`, `bluepad32_get_gamepad` / `bluepad32_get_gamepad_count`). **Phase B:** gamepad → **HID keyboard** (D-pad/buttons) and **left stick → mouse** via `bt_hid_bridge` / `KeyboardPrs` + `MousePrs` (see [gamepad-support.md](gamepad-support.md)). OLED **Devices** / **Bluetooth names:** BT gamepad **count**, **G1** name, and optional live **`BT GP:`** legend when a pad is connected.

## 1.0.17

- **Build speed:** `build_all.sh`, `./build.sh`, and `make` now share a **single** Pico SDK checkout under **`.pico-sdk/pico-sdk`** (and **picotool** under **`.pico-sdk`**) when **`PICO_SDK_PATH`** is not set—avoiding repeated SDK/picotool downloads for each `build-*` directory. See `scripts/lib/build_common.sh` and `./scripts/cmake_with_pico_sdk.sh`. **`./scripts/cleanup_build_artifacts.sh --include-sdk-cache`** removes **`.pico-sdk`** when you need a full reset.

## 1.0.16

- **Product rename:** CMake project/target **`BT-USB-ADB-Adapter-firmware`**, UF2 outputs (`BT-USB-ADB-Adapter-firmware*.uf2`), boot banner, Bluetooth device name, and top-level docs now use **BT-USB-ADB-Adapter**. [HIDHopper.md](HIDHopper.md), [led-support.md](led-support.md), and provenance text elsewhere still refer to **HIDHopper** where useful.

## 1.0.15

- **Repository cleanup for this fork:** The tree is being focused on this **ultramegausb** Apple ADB adapter firmware (USB + Bluetooth via Pico / Pico 2), not on legacy **ADBuino** Arduino hardware or **HIDHopper** retail hardware design drops.
- **Removed ADBuino-era firmware paths:** Unused ADBuino/PlatformIO Arduino target sources, the old `adbuino` CI workflow, and related utilities that only applied to that stack.
- **Removed HIDHopper hardware-design artifacts:** Legacy case CAD, CC–NC KiCad/gerber bundles, and extra product images that were not used by this firmware project. Pinout images useful for DIY wiring (`adb_pinout.png`, Pico pinout) are retained under `images/`.
- **Documentation layout:** User-facing docs live under `docs/` (including migrated `HIDHopper.md`, `adb.md`, and new notes such as `led-support.md`). The old top-level `doc/` folder is gone; `README` links updated accordingly.
- **License text:** `LICENSE` now describes this fork’s goals and third-party stack (including TinyUSB and Bluepad32).
- **Mouse SRQ suppression default:** `ADB_IIGS_MOUSE_SUPPRESS_SRQ` is now **ON** by default in CMake (improves IIgs BASIC and general behavior; use `-DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF` for legacy mouse SRQ). `build_all.sh` no longer passes this flag explicitly.

## 1.0.14

- **IIgs mouse SRQ suppression:** `build_all.sh` now enables `ADB_IIGS_MOUSE_SUPPRESS_SRQ=ON` so the mouse path does not extend SRQ on the IIgs. This prevents the BASIC loop slowdown when moving the mouse.
- **Validation:** confirmed improved behavior on both Apple IIgs (Taifun Boot) and an ADB Mac Quadra.

## 1.0.10

- **IIgs mouse movement tuning:** Added build-time option `ADB_MOUSE_ACCUMULATE_DELTAS` to control how mouse `dx/dy` is handled between ADB polls.
  - `ON` (default): accumulate deltas with saturation (reduces dropped micro-movement and pointer jerk on some IIgs apps).
  - `OFF`: legacy behavior (latest delta wins between polls).
- **IIgs attention floor tuning:** Added build-time setting `ADB_ATTENTION_LO_MIN_US` (default `500`) so attention timing can be A/B tested without source edits.
- **A/B test script:** Added `scripts/build_attention_ab.sh` to build two UF2s with identical firmware except attention floor (`500` vs `450`), now defaulting to `pico2_w` with `PICO_BOARD` override support.
- **Debugging docs:** Updated `docs/iigs-debugging.md` with mouse accumulation A/B guidance and exact CMake flags.
- **Cross-host validation:** The same IIgs-tuned firmware has been tested on an ADB Mac Quadra and showed improved behavior there as well, indicating the receive-path robustness changes are beneficial beyond IIgs-only scenarios.

## 1.0.7

- **Mouse wheel support:** Scroll wheel from USB and Bluetooth mice is now supported. ADB has no native wheel, so wheel is emulated as Up/Down arrow key presses (one key event per wheel tick). Works with both USB HID and Bluepad32 Bluetooth mice.

## 1.0.6

- **ADB disconnect display:** When the adapter is unplugged from the ADB bus, the splash now correctly shows `ADB: --` after about 2 seconds (connection state is only updated when a valid command is received from the bus).
- **Display refresh:** Splash no longer redraws on every SRQ or collision change, only when connection or device IDs change, avoiding full-screen I2C updates during mouse movement and improving responsiveness.
- **Build script:** `build_all.sh` now auto-sets the Pico SDK: it tries common paths (`~/pico/pico-sdk`, `~/pico-sdk`, etc.) and falls back to `PICO_SDK_FETCH_FROM_GIT=ON` if none are found.

## 1.0.5

- **ADB status line on splash (OLED):** When connected, the bottom line shows `ADB: K# M# G#` (keyboard, mouse, and gamepad device IDs). Two optional suffixes indicate bus state:
  - **S** — Service request (SRQ): keyboard or mouse has data pending and is requesting service from the host.
  - **!** — Collision: an ADB bus collision was detected.
  Example: `ADB: K2 M3 G0 S` means connected with kbd 2, mouse 3, no gamepad, and a device has an SRQ pending.

## 1.0.2

- **Right mouse button (ctrl-click) no longer hangs the device.**  
  When the right button was used in ctrl-click mode (default for compatibility with System 6 / System 7), the firmware could deadlock waiting for the keyboard queue to drain from inside the mouse report handler. The blocking waits were removed; Ctrl and click events are enqueued and sent by the main loop, so ctrl-click behaviour is unchanged and the device no longer hangs.

## 1.0.1

- Firmware version numbering: single source of truth in `CMakeLists.txt`; banner and identity show “HIDHopper ADB” and version.
- Flash settings sector moved to avoid overlap with BTstack TLV region on wireless builds.

## 1.0.0

- **Supported boards:** Pico, Pico W, Pico 2, and Pico 2 W. Use `build_all.sh` to build firmware for all four.
- **Bluepad32 support for Bluetooth keyboards and mice** on Pico W and Pico 2 W (boards with CYW43). Bluetooth HID devices are discovered and paired via [Bluepad32](https://github.com/ricardoquesada/bluepad32); keyboard and mouse reports feed into the same ADB pipeline as USB, using the existing parsers and register handling. Up to 2 BT keyboards and 2 BT mice are supported (first of each is active). Build with `pico_w` or `pico2_w` to enable; default builds remain USB-only for non‑wireless boards.
