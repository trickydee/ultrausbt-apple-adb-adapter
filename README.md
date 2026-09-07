# UltraUSBT Apple ADB Adapter

**UltraUSBT** (USB + BT) is open firmware and DIY hardware that bridges modern HID peripherals and vintage Apple Desktop Bus (ADB) machines — and the other way around.

Target MCU: **Raspberry Pi Pico / Pico W / Pico 2 / Pico 2 W**. Reference hardware lives in the [`kicad/`](kicad/) directory (KiCad schematics and PCB for the dual-port ADB pass-through board). Board wiring and GPIO notes: [`docs/hardware.md`](docs/hardware.md).

Firmware CMake product name: **BT-USB-ADB-Adapter**.

---

## What it does

### ADB → Mac (device mode) — default

Use a **modern USB or Bluetooth** keyboard, mouse, or trackball on a vintage ADB host:

- **68K Macintosh** (Classic / LC / Quadra-class ADB Macs)
- **Early PowerPC** ADB Macs
- **Apple IIgs**

**Inputs**

- USB keyboards and mice (TinyUSB host)
- Bluetooth keyboards, mice, and one gamepad slot on Pico W / Pico 2 W ([Bluepad32](https://github.com/ricardoquesada/bluepad32))
- Optional OLED UI for device map, BT pairing clear (`˄+˯`), and mode switching

**Bus coexistence**

- Dual Mini-DIN ADB ports are wired as a **pass-through / daisy-chain**: the adapter can share the bus with real ADB keyboards, mice, trackballs, and other devices
- Hub / address-relocation logic helps USB/BT emulation coexist with a chained pointing device (see [`docs/adb-passthrough-hub.md`](docs/adb-passthrough-hub.md))

### ADB → USB (host mode)

Use **classic Mac ADB accessories** on a modern USB host (PC or Mac):

- Plug ADB keyboard / mouse / trackball into the adapter
- Present them to the computer as a standard USB HID keyboard + mouse
- Switch on the OLED with **`*`** (quick toggle) or the **ADB Mode** screen (`#` to navigate, `˄`/`˯` to select, `#` to apply)
- Persisted in flash; use the unified Pico 2 W host UF2 from `./build-all.sh`

Setup notes: [`docs/adb-host-mode.md`](docs/adb-host-mode.md).

---

## Quick start

**Device mode** (USB/BT → vintage Mac), *before* powering on the Mac:

1. Flash `dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2` from `./build-all.sh` (or a board-specific build)
2. Connect USB and/or pair Bluetooth devices (recommended BT order: **mouse → keyboard → gamepad** — [`docs/bluetooth-pairing.md`](docs/bluetooth-pairing.md))
3. Plug the adapter into the ADB bus (optional second port for pass-through devices)
4. Power on the Mac  
   **No hot-plug** of ADB or USB while the host is running

**Host mode** (ADB accessories → PC/Mac):

1. Vintage Mac **off and disconnected**
2. ADB keyboard/mouse on the adapter; remove USB-A peripherals (BT is not bridged in host mode)
3. Select **ADB → USB** (`*` or Mode screen)
4. Connect the Pico’s native USB to the modern computer

---

## Hardware

| Item | Location |
|------|----------|
| Reference KiCad project (schematic / PCB) | [`kicad/`](kicad/) |
| Pinout, power, GPIO map | [`docs/hardware.md`](docs/hardware.md) |
| DIY board | Dual ADB Mini-DIN, BSS138 level shift, Pico (2 W recommended for BT + host image) |

---

## Documentation

| Doc | Topic |
|-----|--------|
| [`docs/troubleshooting.md`](docs/troubleshooting.md) | Common issues, BT pairing, host timing |
| [`docs/bluetooth-pairing.md`](docs/bluetooth-pairing.md) | Multi-device Bluetooth tips |
| [`docs/adb-host-mode.md`](docs/adb-host-mode.md) | ADB → USB mode |
| [`docs/adb-passthrough-hub.md`](docs/adb-passthrough-hub.md) | Pass-through / hub behaviour |
| [`docs/gamepad-support.md`](docs/gamepad-support.md) | BT gamepad (Phase B) |
| [`docs/release-notes.md`](docs/release-notes.md) | Version history |
| [`docs/iigs-debugging.md`](docs/iigs-debugging.md) | IIgs timing notes |

---

## Build and flash

Intended for a Linux (or similar) environment with the [Pico SDK](https://github.com/raspberrypi/pico-sdk).

```bash
./build-all.sh          # Pico 2 W release + debug UF2s (+ adbmon) → dist/
./build.sh              # quick single-board dev build
```

If `PICO_SDK_PATH` is unset, scripts clone the SDK once under `.pico-sdk/`. Details and flash steps are in the sections below and in [`docs/changes.md`](docs/changes.md).

Release UF2 (typical):

- `dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2` — device + host toggle  
- `dist/BT-USB-ADB-Adapter-firmware-pico2_w-host-debug.uf2` — UART debug @ 115200 on GP0  

Hold **BOOTSEL**, plug USB, drag the `.uf2` onto the `RPI-RP2` drive.

Optional upstream TinyUSB submodule: `git submodule update --init --recursive`.

---

## Lineage

Firmware descends from the adbuino / QuokkADB / HIDHopper ADB line (GPL):

| Stage | Project | Link |
|-------|---------|------|
| ADB protocol | tmk_keyboard | [tmk/tmk_keyboard](https://github.com/tmk/tmk_keyboard) |
| PS/2 → ADB | bbraun adbduino | [synack.net](http://synack.net/svn/adbduino/) |
| PS/2 updates | Difegue | [Chaotic-Realm](https://github.com/Difegue/Chaotic-Realm) · [article](https://tvc-16.science/adbuino-ps2.html) |
| USB → ADB | akuker adbuino | [akuker/adbuino](https://github.com/akuker/adbuino) |
| RP2040 | QuokkADB | [QuokkADB-firmware](https://github.com/rabbitholecomputing/QuokkADB-firmware) |
| Pico product fork | HIDHopper ADB | [TechByAndroda/HIDHopper_ADB](https://github.com/TechByAndroda/HIDHopper_ADB) |
| This tree | UltraUSBT / BT-USB-ADB-Adapter | [trickydee/ultrausbt-apple-adb](https://github.com/trickydee/ultrausbt-apple-adb) |

Also uses [TinyUSB](https://github.com/hathach/tinyusb) and [Bluepad32](https://github.com/ricardoquesada/bluepad32).

---

## License

**GPL-3.0-or-later**. Full text: [`COPYING`](COPYING). Copyright and provenance: [`LICENSE`](LICENSE).

Third-party: TinyUSB (MIT), Bluepad32 (Apache-2.0). Distribute corresponding source with any UF2/binary release.

---

## Protocol references

![ADB Pinout](images/adb_pinout.png)

- [Apple ADB Manager Documentation](https://developer.apple.com/library/archive/documentation/mac/pdf/Devices/ADB_Manager.pdf)
- [ADB Overview](https://www.lopaciuk.eu/2021/03/26/apple-adb-protocol.html)
- [Microchip AN591](http://www.t-es-t.hu/download/microchip/an591b.pdf)
- [TMK ADB wiki](https://github.com/tmk/tmk_keyboard/wiki/Apple-Desktop-Bus)
