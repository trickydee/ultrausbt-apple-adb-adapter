# ADB Host mode

**Status:** Shipped (firmware **2.0.0+**; typing reliability **2.2.3**).  
**UF2:** `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2` from `./build-all.sh`.

Related: [`hardware.md`](hardware.md), [`adb-passthrough-hub.md`](adb-passthrough-hub.md), [`troubleshooting.md`](troubleshooting.md), [`bluetooth-pairing.md`](bluetooth-pairing.md) (BT is **ADB Device** mode only), [`adb-host-mode-capture.md`](adb-host-mode-capture.md).

---

## What it does

| | **ADB Device** (default) | **ADB Host** |
|--|--------------------------|--------------|
| OLED splash | `ADB Dev` | `ADB Host` |
| Mode menu | `ADB Device` | `ADB Host` |
| Role | Emulate ADB kbd/mouse toward a vintage host | Master the ADB bus; present real ADB gear as USB HID |
| USB | Host — USB-A / BT HID in | Device — keyboard + mouse to a modern PC or Mac |
| Typical use | USB/BT → IIgs / Mac 68K / early PowerPC | ADB kbd/mouse/trackball → modern USB host |

**NeXT** has not been tested in ADB Device mode.

Modes are **manual only** (OLED). The Pico has one native USB controller, so USB-A peripherals / Bluepad32 and “ADB Host → PC” cannot run at the same time.

---

## How to switch

1. **`*`** (GP6) from **any** screen — toggles Device ↔ Host, persists, returns to splash (banner updates immediately).
2. **ADB Mode** screen (`#` carousel): **`˄`** = ADB Host, **`˯`** = ADB Device, **`#`** = apply.

Mode is stored in flash (`FlashSettings` / `ADB_SETTINGS_IDX_ADB_MODE`). Cold boot restores the last choice (default Device).

### Switch to ADB Host

1. Vintage Mac / IIgs **off** and **disconnected** from ADB (never two bus masters).
2. Plug ADB keyboard / mouse / trackball into the adapter.
3. Unplug USB-A HID devices (BT is not bridged to the PC in Host mode).
4. Select **ADB Host** (`*` or Mode menu).
5. Connect Pico USB to the modern PC/Mac — expect a composite HID keyboard + mouse.

### Switch back to ADB Device

1. Disconnect from the modern PC (preferred).
2. Select **ADB Device**.
3. Attach USB/BT peripherals; pair BT per [`bluetooth-pairing.md`](bluetooth-pairing.md).
4. Mac / IIgs **off** → plug ADB → power on the vintage host ([`hardware.md`](hardware.md)).

---

## OLED

| Screen | Host-mode notes |
|--------|-----------------|
| Splash | `ADB Host` title; bus summary when devices respond |
| ADB Bus | Configured vs working addresses (`OK` / `--`) |
| ADB Mode | Host / Device select |

---

## Architecture (brief)

| Core | ADB Device | ADB Host |
|------|------------|----------|
| **0** | ADB slave bit-bang, OLED, Bluepad32 | ADB **master** poll, OLED, mode FSM |
| **1** | TinyUSB **host** (`tuh_task`) | TinyUSB **device** (`tud_task` + HID reports) |

On enter Host: global ADB reset, Talk R3 scan, then poll Talk R0 (keyboard ~**8 ms**, pointing ~**24 ms**). Keyboard events are queued as USB HID reports (press+release in one Talk R0 must not coalesce — **2.2.3**).

GPIO: device mode drives DATA high on TX; host TX uses open-collector release in `adb_host_gpio.h` only (shared drive-high restored in **2.1.0** after a 2.0.0 regression — see [`troubleshooting.md`](troubleshooting.md)).

Sources: `adb_mode.cpp`, `adb_host.cpp`, `adb_to_usb.cpp`, `usb_hid_device.c`.

---

## Build

`ADB_HOST_MODE=ON` (default in `./build-all.sh`). Without it, Host UI and bus-master code are omitted.

---

## See also

- [`src/adbmon/README.md`](../src/adbmon/README.md) — passive bus monitor UF2  
- [`fixtures/adb-host/`](fixtures/adb-host/) — Quadra + trackball captures  
- [`archive/adb-shared-gpio-rollback.md`](archive/adb-shared-gpio-rollback.md) — GPIO split history  
