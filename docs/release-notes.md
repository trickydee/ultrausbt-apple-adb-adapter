# HIDHopper ADB – Release notes

## 1.0.10

- **Bluetooth platform note:** Bluetooth pairing and operation can be unstable on **Pico W (RP2040)** (random hangs, first-attempt failures). **Pico 2 W (RP2350)** is the recommended board for reliable Bluetooth keyboards, mice, and gamepads. See `docs/issues.md` and `docs/bluetooth-pairing-and-memory.md`.

## 1.0.9

- **Documentation:** New `docs/bluetooth-pairing-and-memory.md` summarizing Core 1 pause timing, BTstack/Bluepad32 config, and memory/XIP settings compared with amigahid-pico and ultramegausb-atari-st-rpikbd. Includes findings on delay-after-pause, flash TLV vs in-memory bonding, and optional directions for second-device pairing stability.

## 1.0.8

- **Bluetooth pairing stability:** Flash-safe core init and Core 1 pause during BT discovery/connection (aligned with amigahid-pico). Reduces hangs when pairing keyboards, mice, and gamepads; timing delay after device ready before resuming Core 1.
- **Display – Bluetooth gamepad:** Devices screen shows **Gamepad U 0 BT N**. BT names screen shows **G1: &lt;name&gt;** (or G1: --) for the first connected gamepad.
- **ADB joystick visibility:** Game device at address 0x04 always responds to Talk Register 0 with 16 bits so bus inspectors (e.g. tattletech) see the joystick even when no data is pending.

## 1.0.7

- **ADB joystick (Sidewinder 3D Pro):** One Bluetooth gamepad is exposed on the ADB bus at address 0x04 as a Microsoft Sidewinder–compatible device. Supported gamepads include **Sony DualSense (PS5)**, **DualShock 4 (PS4)**, **Google Stadia** (after [BLE firmware update](https://support.google.com/stadia/answer/12475512)), Nintendo Switch Pro, and other Bluepad32-compatible controllers. See `docs/supported-gamepads.md`.
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
