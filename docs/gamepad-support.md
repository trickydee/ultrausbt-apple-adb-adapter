# Gamepad support (Bluetooth first, USB later)

This document tracks **ultrausbt-Apple-ADB-adapter** gamepad work: goals, mapping options, flash/timing notes, and implementation status. For BT pairing order and multi-device tips, see [`bluetooth-pairing.md`](bluetooth-pairing.md).

## Goals

- **Bluetooth (first):** Accept at least **one** Bluetooth gamepad via Bluepad32 (Pico W / Pico 2 W).
- **USB (later):** Feed the same logical layer from TinyUSB HID (pattern: `usb_controllers/` in the local **amigahid-pico** reference tree + host stack).
- **Status UI:** The **Devices** screen shows how many gamepads are connected on **USB** vs **Bluetooth** (the `BT` number in `Gamepad U … BT …` is a **count**, 0 or 1). **`G1`** on the Bluetooth names screen is the first gamepad’s **name slot**, not a separate counter. The **`BT GP:`** line is **not** a count: it is a **live legend** of which controls are active (for debugging). You can hide or simplify it later if you prefer a cleaner UI.
- **ADB output (Phase B):** D-pad and buttons → **HID keyboard** via `KeyboardPrs`; **left stick** → **mouse** via `MousePrs` (see default map below).

## Switchable modes (implementation plan)

Behavior follows the **real Gravis MouseStick II** split documented in `docs/gravis_mousestick_ii.md` and `docs/adb_device_list.md` (address **0x3**, handler **0x01** initially, **0x23** after the Gravis cdev switches).

| Mode | When it applies | Behavior |
|------|-----------------|----------|
| **1 – MouseStick-like (0x01)** | Default; adapter presents as a relative device with **handler ID 0x01** | **Mouse-style movement** and **buttons** mapped through the existing Phase B path: keyboard keys + `MousePrs` (see default map below). Matches “stick as mouse + keys” before any joystick driver runs. **Status: implemented (Phase B).** |
| **2 – Native MouseStick II (0x23)** | **Host-driven:** the Mac loads the **Gravis cdev** and performs the **ADB handler switch** from **0x01** to **0x23** | Firmware responds with **MouseStick II** register data: **Talk 1** protocol id (`0x03 0x00` / `0x04 0x00`) and **Talk 0** in **7-byte** or **3-byte** form per `docs/gravis_mousestick_ii.md` (all five buttons, axes as specified). **Not** selected from the OLED—only when the host actually switches the handler. **Status: planned.** |
| **3 – Custom keymap (“non–MouseStick II”)** | **User toggle** (e.g. OLED/settings), persisted in `FlashSettings` | User-defined **keys** for sticks/D-pad/buttons instead of emulating MouseStick II / Gravis layouts. For users **without** the cdev, or who prefer keyboard-style control. Policy TBD: e.g. ignore or refuse **0x23** while this mode is active so behavior stays predictable. **Status: planned.** |

**Suggested implementation order:** keep **mode 1** stable → implement **mode 2** (detect handler switch, implement Talk 0/1 payloads, SRQ) → add **mode 3** (toggle + keymap storage + UI).

**Detailed plan (real MouseStick II + adbmon fixtures → BT implementation):** [`gravis-mousestick-ii-plan.md`](gravis-mousestick-ii-plan.md).

**Later USB gamepads** should feed the same logical layer so all three modes apply regardless of BT vs USB source.

## Reference: amigahid-pico (historical)

The tree at `/Users/rich/Documents/Code/3rd party/amigahid-pico` (local clone) was the **pre-fix** reference for gamepad plumbing. **Pairing stability** in this firmware follows the **Atari v22.1.0 / Amiga v2.2.11** recipe — not the old amigahid `sleep_ms(50/10)` connect-pause pattern.

**Stadia / Xbox enumeration (this firmware — shipped on `feature/BT-alignment`):**

- **Discovery only:** If CoD **`0x0508`** or name contains **Stadia** / **Xbox** / **XBOX**, Core 1 USB host is paused via `core1_pause_for_bt_enumeration()` → `core1_wait_for_pause_active(20)` → `bt_callback_busy_wait_ms(30)` (`BT_GAMEPAD_DISCOVERY_SETTLE_MS`).
- **`on_device_connected`:** Empty — **no** second pause (avoids stuck `pause_depth`).
- **`on_device_ready`:** Register device; if `core1_get_bt_pause_depth() > 0`: `bt_callback_busy_wait_ms(100)` → `core1_resume_after_bt_enumeration()` — **all device types**, not gamepad-only.
- **`on_device_disconnected`:** Resume if `depth > 0`; `core1_force_release_bt_pause()` before key wipe.
- **Callbacks:** `bt_callback_busy_wait_ms()` only — never `sleep_ms()` on Core 0 during pairing.
- **Core 1:** `tuh_task()` loop uses `__wfe()` while paused; `flash_safe_execute_core_init()` at startup (`quokkadb.cpp`).
- **Constants:** `include/bt_pairing_config.h` — settle 30 ms, pre-resume 100 ms, watchdog 45 s, post-ready ADB settle 2500 ms.

**Do not** pause for generic `"gamepad"` name strings — reduces false pauses.

See [`BT_PAIRING_APPLE_ADB.md`](archive/BT_PAIRING_APPLE_ADB.md), [`troubleshooting.md`](troubleshooting.md) § Bluetooth.

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
| **3 – ADB joystick** | Emulate an **ADB joystick** (e.g. Gravis MouseStick II at handler **0x23**). | **High** | Covered by **Switchable modes** above: **mode 2** (native protocol) + **mode 3** (custom keys). Needs handler switch detection, Talk 0/1, and testing on real Macs / IIgs with the Gravis cdev where applicable. |

**Recommendation:** **Mode 1** is shipped as Phase B. **Mode 2** is the next major milestone for Gravis-accurate games; **Mode 3** follows for users who want remapping without the cdev.

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

**Left stick (analog):** X/Y drive **mouse pointer deltas** via `MousePrs`. Each gamepad report **replaces** the pending stick delta (USB/BT mice still **accumulate** between ADB polls). Releasing the stick to centre clears pending movement immediately. D-pad still sends arrow keys only.

### Tuning: gamepad stick → mouse (`bt_hid_bridge.cpp`)

Constants at the top of `bt_hid_bridge.cpp` (rebuild required after change):

| Constant | Default | Effect |
|----------|---------|--------|
| **`GP_MOUSE_MAX_DELTA`** | `40` | Maximum pointer delta **per axis per gamepad report** (after deadzone). Higher = faster cursor per stick deflection. Range is clamped to boot-HID ±127 before ADB encoding (7-bit movement is halved again in `adbmouseparser.cpp`). |
| **`GP_MOUSE_DEADZONE`** | `64` | Stick deflection below this (Bluepad32 virtual axis ±512) is treated as centred. Wider deadzone reduces drift and slow creep when the stick springs back; too wide feels sluggish near centre. |

Gamepad stick uses **replace** semantics (not `ADB_MOUSE_ACCUMULATE_DELTAS`). USB and Bluetooth **mice** still accumulate between ADB host polls — see `ADB_MOUSE_ACCUMULATE_DELTAS` in [`iigs-debugging.md`](iigs-debugging.md).

**Mouse clicks (gamepad):** **L1** (`BUTTON_SHOULDER_L`) → **left button**; **R2** / right trigger (`BUTTON_TRIGGER_R`) → **right button**. These are not sent as keyboard keys. A button latch keeps L1/R2 pressed across stick-movement frames (same idea as BLE mice). While L1/R2 is held, each gamepad report refreshes button state even if the stick is centred.

**USB mouse wheel:** Boot-protocol HID is buttons + X + Y only; a few mice add a 4th wheel byte and `usbhost.cpp` reads it when `len >= 4`. Most wheel mice need report-protocol parsing (not implemented yet — switching protocol broke movement, so wheel is deferred). Bluetooth mice use Bluepad32’s `scroll_wheel` field. Wheels are emulated as Up/Down arrow keys in `platformmouseparser.cpp`.

### Bluetooth mouse buttons during drag

BLE mice often send **movement-only** HID reports with no button field. Stock Bluepad32 cleared button state at the start of each report, which broke drags. We patch `uni_hid_parser_mouse.c` to **persist buttons** across reports and to clear each button bit when its usage arrives with `value == 0`. `bt_hid_bridge.cpp` keeps a button latch OR'd into each synthetic HID report so movement frames cannot spuriously release the button (without overwriting USB button state).

**ADB timing — do not service BLE during a transaction.** The adapter must answer Talk register 0 within the ADB Tlt (stop-to-start) window of ~140–260 µs, and bit cells are 70–130 µs. Running `bluepad32_poll()` (BTstack) during `ReceiveCommand()` bit waits or between receiving Talk R0 and sending the register blows these timings, so the host drops the response — felt as jerky movement and lost slow-drag motion (only large/fast deltas survived). BLE is serviced **once per main-loop pass** (like the original main branch); when connected to a Mac the host polls frequently, so the loop stays responsive without per-bit cooperative polling.

## OLED legend (`BT GP:` line)

Characters are appended only while the control is active: `^` `v` `<` `>` (D-pad), `a` `b` `x` `y`, `L` `R` (shoulders), `1` `2` (triggers), `,` `.` (thumbs), `s` `t` `y` `c` (select/start/system/capture). When nothing is pressed, the line shows `BT GP: --`.

## Implementation status

### Done (Phase A – plumbing)

- Single gamepad slot in `bluepad32_platform.c`: connect / disconnect / `on_controller_data` for `UNI_CONTROLLER_CLASS_GAMEPAD`.
- APIs: `bluepad32_get_gamepad_count()`, `bluepad32_get_gamepad(idx, out)`, `bluepad32_get_device_name('G', idx)`.
- OLED: **Devices** screen and **Bluetooth names** screen show gamepad counts / **G1** name; main loop passes BT gamepad count into `display_set_bt_counts`.

### Done (Phase B – keyboard emulation + OLED viz)

- `process_bluepad32_devices()` maps gamepad → `hid_keyboard_report_t` and merges with BT keyboard via `build_merged_bt_keyboard_report()` before `KeyboardPrs.Parse` at fake BT address `0x80`.
- **Keyboard + gamepad merge:** `bluepad32_peek_keyboard()` / `bluepad32_peek_gamepad()` — always merge when both are connected; gamepad keys suppressed during Core 1 pause when a BT keyboard is connected (`include_gamepad_keys_in_keyboard_report()`).
- **Left stick → mouse:** merged with BT mouse movement in `bt_hid_bridge.cpp` (single `bluepad32_peek_gamepad` read per loop).
- `bluepad32_get_gamepad_visual()` exposes the latest sticks/buttons for the OLED without consuming the `updated` flag used for input.
- `display_poll_bt_gamepad_viz()` redraws **Devices** / **Bluetooth names** only when the live snapshot changes (avoids hammering I2C).

### Planned (Phase C – switchable modes)

- **Mode 2:** Detect ADB **handler switch** to **0x23**; implement **Talk 0 / Talk 1** per `docs/gravis_mousestick_ii.md`; map Bluetooth gamepad axes/buttons into 7-byte / 3-byte reports as appropriate.
- **Mode 3:** User **toggle** + persisted **custom keymap** (extend `FlashSettings` carefully; re-verify flash layout).
- USB HID gamepads via TinyUSB → same bridge as Bluetooth.
- **Core 1 pause / pairing stability** — **Done** in **2.2.1**; see [`FUTURE_WORK.md`](FUTURE_WORK.md), [`archive/BT_PAIRING_APPLE_ADB.md`](archive/BT_PAIRING_APPLE_ADB.md), [`troubleshooting.md`](troubleshooting.md) § Bluetooth.

## Version

Document aligned with firmware **2.2.1** (`feature/BT-alignment`); see [`release-notes.md`](release-notes.md).
