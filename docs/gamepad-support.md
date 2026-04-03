# Gamepad support (Bluetooth first, USB later)

This document tracks **BT-USB-ADB-Adapter** gamepad work: goals, mapping options, flash/timing notes, and implementation status.

## Goals

- **Bluetooth (first):** Accept at least **one** Bluetooth gamepad via Bluepad32 (Pico W / Pico 2 W).
- **USB (later):** Feed the same logical layer from TinyUSB HID (pattern: `usb_controllers/` in the local **amigahid-pico** reference tree + host stack).
- **Status UI:** The **Devices** screen shows how many gamepads are connected on **USB** vs **Bluetooth** (the `BT` number in `Gamepad U … BT …` is a **count**, 0 or 1). **`G1`** on the Bluetooth names screen is the first gamepad’s **name slot**, not a separate counter. The **`BT GP:`** line is **not** a count: it is a **live legend** of which controls are active (for debugging). You can hide or simplify it later if you prefer a cleaner UI.
- **ADB output (Phase B):** D-pad and buttons → **HID keyboard** via `KeyboardPrs`; **left stick** → **mouse** via `MousePrs` (see default map below).

## Reference: amigahid-pico

The tree at `/Users/rich/Documents/Code/3rd party/amigahid-pico` (local clone) implements:

- `bt_gamepad_storage_t`, `MAX_BT_GAMEPADS`, `uni_hid_device_is_gamepad()`, `UNI_CONTROLLER_CLASS_GAMEPAD` in `on_controller_data`.
- **Stadia / Xbox enumeration:** In `bluepad32_platform.c`, when a gamepad-class device is **discovered** (COD `0x0508` or name hints) or an **Xbox / Google (Stadia) VID** device **connects**, the project **pauses Core 1’s quadrature mouse loop** (`amiga_quad_mouse_pause_core1`). After **`on_device_ready`** for a gamepad, it **`sleep_ms(50)`** for Xbox/Stadia ( **`10ms`** for other pads) then **resumes** Core 1. Core 1 also calls **`flash_safe_execute_core_init()`** so BTstack TLV flash work does not deadlock the second core. **`on_controller_data`** ignores gamepad reports until **`storage->connected`** is set in **`on_device_ready`** (enumeration complete).
- **This firmware (ported behavior):** Core 1 runs **TinyUSB `tuh_task()`**, not quadrature. **`bt_host_coop.c`** exposes a flag so Core 1 **skips `tuh_task`** while the flag is set (same windows as above: discovery / connect / until gamepad ready + delay). Core 1 calls **`flash_safe_execute_core_init()`** once at startup (see `quokkadb.cpp`).

## Flash and BTstack

- **BTstack link keys / TLV** live in **`pico_btstack`**’s flash bank: see `PICO_FLASH_BANK_STORAGE_OFFSET` and `PICO_FLASH_BANK_TOTAL_SIZE` in the Pico SDK header `pico/btstack_flash_bank.h` (defaults: **8 KiB** in the last part of flash; on **RP2350 + A2** the bank shifts up so the **last sector stays free** for errata workarounds).
- **Adapter settings** (`flashsettings.cpp`) use **one sector immediately below** that TLV region:  
  `offset = <TLV base from the same #if as btstack_flash_bank.h> - FLASH_SECTOR_SIZE`  
  so settings **never** share a sector with Bluetooth pairing storage. The old formula `_capacity - 3 * sector` matched **RP2040** only and could **overlap** TLV on **RP2350** when the A2 layout is enabled.
- Settings saves use **`multicore_lockout_*`** around erase/program.
- **Do not** trigger flash erase/program from Bluetooth callbacks or from code that runs inside unpredictable stack timing.
- Persisted **key maps** (future) must extend `FlashSettings` carefully and re-verify layout after linker changes.

## Mapping options (ADB behavior)

| Option | Description | Effort | Notes |
|--------|-------------|--------|--------|
| **1 – Keyboard** | Map D-pad / buttons to **USB HID keycodes** → existing `KeyboardPrs` / ADB keyboard. | **Low** | Good MVP; no analog; key rollover rules apply. |
| **2 – Mouse** | Map sticks to **mouse deltas** → `MousePrs`. | **Medium** | Useful for pointer control; conflicts if a real mouse is active—needs policy. |
| **3 – ADB joystick** | Emulate an **ADB joystick** (e.g. Gravis Mousestick–class behavior). | **High** | Today’s firmware models **keyboard + mouse** ADB devices only; this needs **new device address**, registers, and testing on real Macs / IIgs. |

**Recommendation:** Ship **(1)** first, add **(2)** as an optional profile, treat **(3)** as a dedicated milestone after MVP.

## Default keyboard map (Phase B)

Implemented in `bt_hid_bridge.cpp` (merged with BT keyboard reports when both update in the same poll; up to six non-modifier keys).

| Control | HID usage |
|--------|-----------|
| D-pad | Arrow keys |
| A | Space |
| B | Escape |
| X / Y | Z / X |
| Shoulder R | E |
| Trigger L (L2) | 1 |
| Thumb L / R | Comma / Period |
| Start / Select / System / Capture | Enter / Tab / F1 / F12 |

**Left stick (analog):** X/Y drive **mouse pointer deltas** via `MousePrs`. Sensitivity is set by **`GP_MOUSE_MAX_DELTA`** (max per axis per report) and **`GP_MOUSE_DEADZONE`** in `bt_hid_bridge.cpp` (lower max = slower cursor). Deltas are **added** to a connected **Bluetooth mouse** in the same poll if both move. D-pad still sends arrow keys only.

**Mouse clicks (gamepad):** **L1** (`BUTTON_SHOULDER_L`) → **left button**; **R2** / right trigger (`BUTTON_TRIGGER_R`) → **right button**. These are not sent as keyboard keys. Edge transitions are tracked so **button release** is delivered to the host.

## OLED legend (`BT GP:` line)

Characters are appended only while the control is active: `^` `v` `<` `>` (D-pad), `a` `b` `x` `y`, `L` `R` (shoulders), `1` `2` (triggers), `,` `.` (thumbs), `s` `t` `y` `c` (select/start/system/capture). When nothing is pressed, the line shows `BT GP: --`.

## Implementation status

### Done (Phase A – plumbing)

- Single gamepad slot in `bluepad32_platform.c`: connect / disconnect / `on_controller_data` for `UNI_CONTROLLER_CLASS_GAMEPAD`.
- APIs: `bluepad32_get_gamepad_count()`, `bluepad32_get_gamepad(idx, out)`, `bluepad32_get_device_name('G', idx)`.
- OLED: **Devices** screen and **Bluetooth names** screen show gamepad counts / **G1** name; main loop passes BT gamepad count into `display_set_bt_counts`.

### Done (Phase B – keyboard emulation + OLED viz)

- `process_bluepad32_devices()` maps gamepad → `hid_keyboard_report_t` and calls `KeyboardPrs.Parse` at fake BT address `0x80` (same as BT keyboard).
- **Left stick → mouse:** merged with BT mouse movement in `bt_hid_bridge.cpp` (single `bluepad32_get_gamepad` read per loop).
- `bluepad32_get_gamepad_visual()` exposes the latest sticks/buttons for the OLED without consuming the `updated` flag used for input.
- `display_poll_bt_gamepad_viz()` redraws **Devices** / **Bluetooth names** only when the live snapshot changes (avoids hammering I2C).

### Later

- USB HID gamepads via TinyUSB → same bridge.
- Optional **Core 1 pause / deferred display** if a specific BT controller misbehaves during pairing (port patterns from amigahid-pico).

## Version

Document aligned with firmware **1.0.17+**; bump `docs/release-notes.md` when gamepad output ships to users.
