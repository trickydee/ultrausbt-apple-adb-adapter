# HIDHopper ADB – Release notes

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
