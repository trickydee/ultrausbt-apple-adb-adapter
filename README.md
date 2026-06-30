# BT-USB-ADB-Adapter

**BT-USB-ADB-Adapter** is a modified (forked) version of adbuino and QuokkADB. It is a Raspberry Pi Pico based device that converts USB keyboard and mouse inputs (and Bluetooth on Pico W / Pico 2 W) to the Apple Desktop Bus (ADB) standard.

## Provenance

This firmware is a fork of [adbuino](https://github.com/Difegue/Chaotic-Realm) and [QuokkADB](https://github.com/rabbitholecomputing/QuokkADB-firmware), extended for USB + Bluetooth on Raspberry Pi Pico boards. It targets the **ultramegausb DIY ADB adapter** — see [`docs/hardware.md`](docs/hardware.md).

# Usage

See [`docs/hardware.md`](docs/hardware.md) for board wiring, power, and setup. **ADB device mode** (USB/BT → vintage Mac) is the default.

# Quick usage

- **Device mode** (default), *before* turning on the Mac:
   - Plug in your USB keyboard and/or mouse (or pair Bluetooth on Pico W / Pico 2 W)
   - Plug the adapter into the ADB bus
   - Power on the Mac
   - **No hot-plug** — do not unplug from ADB or remove USB devices while the Mac is running
- **Host mode** (ADB accessories → PC): flash **`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`** from `./build-all.sh`; switch mode on the OLED (**ADB → USB**). Mac **off and disconnected**; power the Pico from USB (see [`docs/hardware.md`](docs/hardware.md)).

# Background

This is a fork of Difegue's version of the [adbuino](https://github.com/Difegue/Chaotic-Realm), which was a modified version of [bbraun's](http://synack.net/svn/adbduino/) PS/2 to ADB arduino sketch, with some extra code added to alleviate issues with his own PS/2 keyboard.  For Difegue's original write-up, please read more info [here.](https://tvc-16.science/adbuino-ps2.html).

## Project documentation

- [docs/troubleshooting.md](docs/troubleshooting.md) — BT mouse regression, host timing, flash images
- [docs/gamepad-support.md](docs/gamepad-support.md) — gamepad support roadmap (Bluetooth / USB, ADB mapping options)

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
- [TinyUSB Library](https://github.com/raspberrypi/tinyusb)
- [MiSTER adb hardware emulation](https://github.com/mist-devel/plus_too/blob/master/adb.v)

## Development resources
- [Running OpenOCD without root](https://forgge.github.io/theCore/guides/running-openocd-without-sudo.html)

## Hardware Links
- [ADB Connector - mouser](https://www.mouser.com/ProductDetail/TE-Connectivity/5749181-1?qs=XlZqES4cpWbRcAMR%2FcJqkQ%3D%3D)
