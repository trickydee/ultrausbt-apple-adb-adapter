# Apple ADB USB & Bluetooth Adapter

## Overview

This project uses a Raspberry Pi Pico to connect modern USB and Bluetooth devices such as keyboards, mice, trackballs, and gamepads to classic Apple Desktop Bus (ADB) computers — without needing USB on the vintage machine.

It targets **68K Macintosh** (Classic / LC / Quadra-class), **early PowerPC** ADB Macs, and the **Apple IIgs**. **NeXT** machines have **not** been tested yet.

The adapter is bi-directional (OLED labels **ADB Dev** / **ADB Host** on the homescreen; **ADB Device** / **ADB Host** on the Mode menu):

* **ADB Device** mode (default): attach USB and Bluetooth devices to an ADB host — Apple IIgs, Mac 68K, or early PowerPC ADB Macs
* **ADB Host** mode: connect ADB devices (keyboard, mouse, trackball, …) to a USB host such as a modern PC or Mac

Dual Mini-DIN ADB ports are wired as a **pass-through**: you can daisy-chain real ADB keyboards, mice, trackballs, and other devices alongside the adapter’s USB/BT emulation (see [`docs/adb-passthrough-hub.md`](./docs/adb-passthrough-hub.md)).

On a **Pico 2 W** you can mix USB and Bluetooth devices. USB-only builds work on Pico / Pico 2 as well. Prefer **Pico 2 W** for the full feature set (Bluetooth + Device/Host toggle).

**This firmware descends from the adbuino / QuokkADB / HIDHopper ADB line** (GPL). Upstream projects to credit:

* [akuker/adbuino](https://github.com/akuker/adbuino) · [QuokkADB-firmware](https://github.com/rabbitholecomputing/QuokkADB-firmware) · [HIDHopper_ADB](https://github.com/TechByAndroda/HIDHopper_ADB)

Earlier roots include [bbraun’s adbduino](http://synack.net/svn/adbduino/), [Difegue’s updates](https://tvc-16.science/adbuino-ps2.html), and [tmk_keyboard](https://github.com/tmk/tmk_keyboard) ADB code.

Current firmware: **v2.2.3** (`main`) · [Release notes](./docs/release-notes.md) · License: [GPL-3.0-or-later](./LICENSE) · [COPYING](./COPYING)

Firmware CMake product name: **ultrausbt-Apple-ADB-adapter**.

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

## ADB Device mode (USB/BT → vintage ADB host)

Default mode. Attach USB and Bluetooth keyboards, mice, trackballs, and (on Pico W / Pico 2 W) a gamepad to an ADB host:

* Apple IIgs
* Macintosh 68K (Classic / LC / Quadra-class)
* Early PowerPC ADB Macs

**NeXT** machines have **not** been tested yet.

Pass-through port: real ADB devices can share the bus with the adapter.

## ADB Host mode (ADB devices → modern USB host)

Connect ADB devices (keyboard, mouse, trackball, …) to a USB host such as a modern PC or Mac. The adapter polls the ADB bus and presents them as USB HID.

* Switch with **`*`** from any OLED screen, or use the **ADB Mode** page (`#` carousel → Mode; `˄`/`˯` select **ADB Host** / **ADB Device**, `#` apply). Homescreen shows **ADB Host** or **ADB Dev**.
* Vintage Mac / IIgs should be **off and disconnected**; remove USB-A peripherals while in ADB Host mode (Bluetooth is not bridged to the PC in this mode).
* Details: [`docs/adb-host-mode.md`](./docs/adb-host-mode.md).

# OLED UI

An SSD1306 OLED and four buttons are supported on the UltraUSBT ADB board (optional on a bare Pico, but recommended):

| Label | Role |
|-------|------|
| **˄** (Up) | Mode screen: select **ADB Host**; with ˯: clear BT pairings |
| **˯** (Down) | Mode screen: select **ADB Device**; with ˄: clear BT pairings |
| **#** (Center) | Advance screen carousel; Mode screen: apply |
| **\*** | Toggle ADB Device ↔ ADB Host from any screen |

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

* `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2` — release image (**ADB Device** and **ADB Host**)
* `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host-debug.uf2` — UART debug @ 115200 on GP0

If `PICO_SDK_PATH` is unset, the scripts clone the Pico SDK once under `.pico-sdk/`. Optional upstream TinyUSB: `git submodule update --init --recursive`.

# Documentation

| Doc | Topic |
|-----|--------|
| [`docs/release-notes.md`](./docs/release-notes.md) | Firmware changelog |
| [`docs/troubleshooting.md`](./docs/troubleshooting.md) | Common issues, BT pairing, host timing |
| [`docs/bluetooth-pairing.md`](./docs/bluetooth-pairing.md) | Multi-device Bluetooth tips |
| [`docs/adb-host-mode.md`](./docs/adb-host-mode.md) | ADB Host mode (ADB devices → modern USB host) |
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
* Atari Mega ST/TT IKBD USB/BT adapter (related ultrausbt / ultrausbt trees)

**This UltraUSBT Apple ADB tree** is maintained as [trickydee/ultrausbt-apple-adb-adapter](https://github.com/trickydee/ultrausbt-apple-adb-adapter).
