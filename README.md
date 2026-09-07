# Apple ADB USB & Bluetooth Adapter

## Overview

This project uses a Raspberry Pi Pico to connect modern USB and Bluetooth devices such as keyboards, mice, trackballs, and gamepads to classic Apple Desktop Bus (ADB) computers — without needing USB on the vintage machine.

It targets **68K Macintosh** (Classic / LC / Quadra-class), **early PowerPC** ADB Macs, and the **Apple IIgs**.

The adapter is bi-directional:

* **Device Mode** (default): modern USB/Bluetooth HID → emulated ADB keyboard and mouse on the vintage Mac / IIgs
* **Host Mode**: real ADB keyboard / mouse / trackball → USB HID keyboard and mouse on a modern PC or Mac

Dual Mini-DIN ADB ports are wired as a **pass-through**: you can daisy-chain real ADB keyboards, mice, trackballs, and other devices alongside the adapter’s USB/BT emulation (see [`docs/adb-passthrough-hub.md`](./docs/adb-passthrough-hub.md)).

On a **Pico 2 W** you can mix USB and Bluetooth devices. USB-only builds work on Pico / Pico 2 as well. Prefer **Pico 2 W** for the full feature set (Bluetooth + host/device toggle).

**This firmware descends from the adbuino / QuokkADB / HIDHopper ADB line** (GPL). Upstream projects to credit:

* [akuker/adbuino](https://github.com/akuker/adbuino) · [QuokkADB-firmware](https://github.com/rabbitholecomputing/QuokkADB-firmware) · [HIDHopper_ADB](https://github.com/TechByAndroda/HIDHopper_ADB)

Earlier roots include [bbraun’s adbduino](http://synack.net/svn/adbduino/), [Difegue’s updates](https://tvc-16.science/adbuino-ps2.html), and [tmk_keyboard](https://github.com/tmk/tmk_keyboard) ADB code.

Current firmware: **v2.2.2** (`main`) · [Release notes](./docs/release-notes.md) · License: [GPL-3.0-or-later](./LICENSE) · [COPYING](./COPYING)

Firmware CMake product name: **BT-USB-ADB-Adapter**.

![ADB Pinout](./images/adb_pinout.png)

## Contributions

This project is open source and I am happy to receive pull requests and issues to further improve capabilities.

## USB device support

* USB HID keyboards
* USB HID mice (and many trackballs that appear as mice)
* Bluetooth keyboards, mice, and one gamepad slot on Pico W / Pico 2 W (see below)

## Game controller support (Bluetooth)

One Bluetooth gamepad can be connected (Bluepad32). **Phase B** maps D-pad/buttons to keyboard keys and the left stick to mouse movement — useful without native ADB joystick firmware. Details: [`docs/gamepad-support.md`](./docs/gamepad-support.md).

Native Gravis MouseStick II (handler **0x23**) is planned; see [`docs/FUTURE_WORK.md`](./docs/FUTURE_WORK.md).

## Bluetooth support (Pico W / Pico 2 W)

Bluetooth keyboards, mice, and gamepads are supported via [Bluepad32](https://github.com/ricardoquesada/bluepad32). You can run a mostly wireless ADB setup, or mix USB and Bluetooth.

### Pairing

1. Put the Bluetooth device into pairing mode.
2. Confirm it on the OLED **Devices** / **Map Devices** screens.
3. Recommended order when pairing several devices (especially on Mac cold boot): **mouse → keyboard → gamepad**.

To clear stored pairing keys, hold **˄ + ˯** (Up + Down) for about five seconds (Map Devices shows `^+~ Clear Pair`).

More detail: [`docs/bluetooth-pairing.md`](./docs/bluetooth-pairing.md).

**Note:** Prefer **Pico 2 W** for Bluetooth builds.

# Usage

## USB

Connect devices to the Pico USB port (use a powered USB OTG hub if you need several). Supported keyboards and mice should enumerate within a few seconds; confirm on the OLED.

**Before powering on the Mac:** plug the adapter into ADB (and any pass-through ADB devices into the second port), attach USB/BT peripherals, then power on the host. **No hot-plug** of ADB or USB while the vintage machine is running.

## Bluetooth

See **Bluetooth support** above. Pairing and clear-keys are OLED-driven.

## Device Mode (USB/BT → vintage Mac / IIgs)

Default mode. Emulates ADB keyboard and mouse so modern peripherals work on:

* 68K ADB Macs
* Early PowerPC ADB Macs
* Apple IIgs

Pass-through port: real ADB devices can share the bus with the adapter.

## Host Mode (ADB accessories → modern PC / Mac)

Run the adapter in reverse: talk to a real ADB keyboard, mouse, or trackball and present them as USB HID to a modern computer.

* Switch with **`*`** from any OLED screen, or use the **ADB Mode** page (`#` carousel → Mode; `˄`/`˯` select, `#` apply).
* Vintage Mac should be **off and disconnected**; remove USB-A peripherals while in Host Mode (Bluetooth is not bridged to the PC in this mode).
* Details: [`docs/adb-host-mode.md`](./docs/adb-host-mode.md).

# OLED UI

An SSD1306 OLED and four buttons are supported on the UltraUSBT ADB board (optional on a bare Pico, but recommended):

| Label | Role |
|-------|------|
| **˄** (Up) | Mode screen: select ADB → Mac; with ˯: clear BT pairings |
| **˯** (Down) | Mode screen: select ADB → USB; with ˄: clear BT pairings |
| **#** (Center) | Advance screen carousel; Mode screen: apply |
| **\*** | Toggle Device ↔ Host mode from any screen |

Carousel (host builds): Splash → Devices → Map Devices → ADB Bus → ADB Mode → Splash.

# Hardware

The [`kicad/`](./kicad/) directory holds the **reference board** design for this project (KiCad schematic / PCB — dual Mini-DIN ADB pass-through, BSS138 level shifting, Pico).

Firmware GPIO and power notes: [`docs/hardware.md`](./docs/hardware.md).

# Building the firmware

Download a pre-built UF2 from Releases when available, or build with `./build-all.sh`:

```bash
# Default: Pico 2 W (device + host toggle) → dist/
./build-all.sh

# Quick single-board dev build
./build.sh
```

Flash the matching `.uf2` from `dist/` (hold **BOOTSEL**, copy to the RPI-RP2 drive).

Typical outputs:

* `dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2` — release image (ADB → Mac and ADB → USB)
* `dist/BT-USB-ADB-Adapter-firmware-pico2_w-host-debug.uf2` — UART debug @ 115200 on GP0

If `PICO_SDK_PATH` is unset, the scripts clone the Pico SDK once under `.pico-sdk/`. Optional upstream TinyUSB: `git submodule update --init --recursive`.

# Documentation

| Doc | Topic |
|-----|--------|
| [`docs/release-notes.md`](./docs/release-notes.md) | Firmware changelog |
| [`docs/troubleshooting.md`](./docs/troubleshooting.md) | Common issues, BT pairing, host timing |
| [`docs/bluetooth-pairing.md`](./docs/bluetooth-pairing.md) | Multi-device Bluetooth tips |
| [`docs/adb-host-mode.md`](./docs/adb-host-mode.md) | ADB → USB host mode |
| [`docs/adb-passthrough-hub.md`](./docs/adb-passthrough-hub.md) | Pass-through / hub behaviour |
| [`docs/gamepad-support.md`](./docs/gamepad-support.md) | BT gamepad mapping |
| [`docs/iigs-debugging.md`](./docs/iigs-debugging.md) | IIgs timing notes |
| [`docs/FUTURE_WORK.md`](./docs/FUTURE_WORK.md) | Roadmap |
| [`docs/hardware.md`](./docs/hardware.md) | Board / GPIO |

# Acknowledgements

**Upstream — please support the original projects:**

* **adbuino** / **QuokkADB** / **HIDHopper ADB** — the ADB-on-Pico lineage this firmware continues
* **tmk_keyboard** — ADB protocol foundations
* **bbraun** and **Difegue** — early adbduino / PS/2 work

The “ultrausbt” name highlights USB + Bluetooth capabilities made possible by TinyUSB, Bluepad32, and the Pico ecosystem.

This fork also relies on:

* Raspberry Pi Pico SDK — RP2040 / RP2350 platform
* TinyUSB by Ha Thach — USB host and device stacks
* Bluepad32 by Ricardo Quesada — Bluetooth HID

**Other UltraUSBT projects**

* [Amiga USB/BT adapter](https://github.com/trickydee/ultrausbt-amiga)
* Atari Mega ST/TT IKBD USB/BT adapter (related ultrausbt / ultramegausb trees)

**This UltraUSBT Apple ADB tree** is maintained as [trickydee/ultrausbt-apple-adb](https://github.com/trickydee/ultrausbt-apple-adb).
