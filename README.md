# BT-USB-ADB-Adapter

**BT-USB-ADB-Adapter** converts USB keyboard and mouse input (and Bluetooth on Pico W / Pico 2 W) to Apple Desktop Bus (ADB) for vintage Macs and the Apple IIgs. It targets the **ultramegausb** DIY adapter — see [`docs/hardware.md`](docs/hardware.md).

## Lineage

This firmware descends from the adbuino / QuokkADB / HIDHopper ADB line:

| Stage | Project | Link |
|-------|---------|------|
| ADB protocol (host → later device) | tmk_keyboard | [tmk/tmk_keyboard](https://github.com/tmk/tmk_keyboard) ([`adb.c`](https://github.com/tmk/tmk_keyboard/blob/master/tmk_core/protocol/adb.c)) |
| Early PS/2 → ADB Arduino | bbraun adbduino | [synack.net adbduino](http://synack.net/svn/adbduino/) · [write-up index](http://synack.net/~bbraun/) |
| PS/2 improvements | Difegue | [Chaotic-Realm / adbuino](https://github.com/Difegue/Chaotic-Realm) · [article](https://tvc-16.science/adbuino-ps2.html) |
| USB host → ADB | akuker adbuino | [akuker/adbuino](https://github.com/akuker/adbuino) |
| RP2040 / QuokkADB | Rabbit Hole Computing | [rabbitholecomputing/QuokkADB-firmware](https://github.com/rabbitholecomputing/QuokkADB-firmware) |
| Pico product fork | HIDHopper ADB | [TechByAndroda/HIDHopper_ADB](https://github.com/TechByAndroda/HIDHopper_ADB) |
| This tree | BT-USB-ADB-Adapter (ultramegausb) | [trickydee/ultramegausb-apple-adb](https://github.com/trickydee/ultramegausb-apple-adb) |

Bluetooth HID uses [Bluepad32](https://github.com/ricardoquesada/bluepad32); USB uses [TinyUSB](https://github.com/hathach/tinyusb) (or the Pico SDK bundle).

## License

This project is **GPL-3.0-or-later**. The full GNU GPL v3 text is in [`COPYING`](COPYING); copyright and provenance notes are in [`LICENSE`](LICENSE).

Earlier trees sometimes shipped a GPLv2 `COPYING` file alongside a GPLv3+ project notice. This repository uses **GPL v3 (or later)** throughout: `COPYING` is the GPLv3 license text.

Third-party components keep their own terms (e.g. TinyUSB **MIT**, Bluepad32 **Apache-2.0**) — see their `LICENSE` files under `src/firmware/`.

When you distribute UF2/binaries, provide corresponding source (this repo or a tagged release that builds that image).

# Usage

See [`docs/hardware.md`](docs/hardware.md) for board wiring, power, and setup. **ADB device mode** (USB/BT → vintage Mac) is the default.

# Quick usage

- **Device mode** (default), *before* turning on the Mac:
   - Plug in your USB keyboard and/or mouse (or pair Bluetooth on Pico W / Pico 2 W — **mouse → keyboard → gamepad** if pairing multiple BT devices; see [`docs/bluetooth-pairing.md`](docs/bluetooth-pairing.md))
   - Plug the adapter into the ADB bus
   - Power on the Mac
   - **No hot-plug** — do not unplug from ADB or remove USB devices while the Mac is running
- **Host mode** (ADB accessories → PC): flash **`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`** from `./build-all.sh`; switch mode on the OLED (`*` toggle or Mode screen). Mac **off and disconnected**; power the Pico from USB (see [`docs/hardware.md`](docs/hardware.md)).

## Project documentation

- [docs/troubleshooting.md](docs/troubleshooting.md) — BT mouse regression, host timing, flash images
- [docs/bluetooth-pairing.md](docs/bluetooth-pairing.md) — BT pairing order, multi-device tips (keyboard + mouse + gamepad)
- [docs/gamepad-support.md](docs/gamepad-support.md) — gamepad support (Bluetooth Phase B shipped; Gravis Phase C planned)
- [docs/release-notes.md](docs/release-notes.md) — version history (2.2.1 BT pairing stability)
- [docs/adb-host-mode.md](docs/adb-host-mode.md) — ADB → USB host mode

# How to build and flash BT-USB-ADB-Adapter

Note: This software is intended to be compiled in an Ubuntu Linux environment.

- **Pico SDK:** Either install [pico-sdk](https://github.com/raspberrypi/pico-sdk) somewhere and set **`PICO_SDK_PATH`**, or leave it unset. **`build-all.sh`**, **`./build.sh`**, and **`make`** run `scripts/lib/build_common.sh`, which clones the SDK **once** into **`.pico-sdk/pico-sdk`** (gitignored) and sets **`PICOTOOL_FETCH_FROM_GIT_PATH`** to **`.pico-sdk`** so picotool is also built in that single tree. Override the tag with **`PICO_SDK_TAG`** (default **`2.2.0`**, keep in sync with `src/firmware/pico_sdk_import.cmake`).
- **From the top level of this project** you can build with:
  - **`./build-all.sh`** — recommended release bundle: unified **Pico 2 W** adapter (device + host toggle), UART debug UF2, and **adbmon** for Pico → `dist/`
  - `make` or `./build.sh` — quick single dev build under `src/firmware/build/`
- To use **upstream TinyUSB** (optional), initialize the submodule first:  
  `git submodule update --init --recursive`  
  Then build as above; the firmware will use the TinyUSB at `src/firmware/tinyusb`. See [docs/changes.md](docs/changes.md).
- Or configure manually with the same environment as the scripts:  
  `./scripts/cmake_with_pico_sdk.sh -B src/firmware/build -S src/firmware` then `cmake --build src/firmware/build -j$(nproc)`
- The build outputs (.uf2, .bin, .elf, etc) will be placed in:
  - `src/firmware/build/src`
- Next, plug the micro-USB side of a USB cable into the Pico running this firmware
- Press the button which is near the micro-USB port
- While holding down the button, plug the USB cable into your computer (then release the button after plugging in)
- You should see an "RPI-RP2" mass storage device appear on your computer
- Drag the `.uf2` file onto that mass storage device (release: `dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2` from `./build-all.sh`, or `src/firmware/build/src/BT-USB-ADB-Adapter-firmware.uf2` from `./build.sh`)
- Once the mass storage device disappears, wait 10 seconds and then you are free to unplug

# References
![ADB Pinout](images/adb_pinout.png)

## Protocol/Software Documentation
- [Apple ADB Manager Documenation](https://developer.apple.com/library/archive/documentation/mac/pdf/Devices/ADB_Manager.pdf)
- [ADB Overview](https://www.lopaciuk.eu/2021/03/26/apple-adb-protocol.html)
- [Microchip Application Note AN591](http://www.t-es-t.hu/download/microchip/an591b.pdf)
- [TMK Documentation](https://github.com/tmk/tmk_keyboard/wiki/Apple-Desktop-Bus)

## Other libraries
- [TinyUSB](https://github.com/hathach/tinyusb)
- [Bluepad32](https://github.com/ricardoquesada/bluepad32)
- [MiSTER adb hardware emulation](https://github.com/mist-devel/plus_too/blob/master/adb.v)

## Development resources
- [Running OpenOCD without root](https://forgge.github.io/theCore/guides/running-openocd-without-sudo.html)

## Hardware Links
- [ADB Connector - mouser](https://www.mouser.com/ProductDetail/TE-Connectivity/5749181-1?qs=XlZqES4cpWbRcAMR%2FcJqkQ%3D%3D)
