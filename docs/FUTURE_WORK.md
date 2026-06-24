# Future work

Tracked improvements and features for **ultramegausb-apple-adb**. For a broader project backlog (joystick, adbmon, Wacom), see also [`todo.md`](todo.md).

---

## 1. Bluetooth pairing stability (Pico W / Pico 2 W)

**Priority:** High  
**Status:** Not started (reference docs in place)  
**Symptom:** Random hangs when pairing BLE devices — especially **gamepad** while keyboard/mouse are connected; intermittent keyboard pairing stalls (e.g. MX Keys M). Heisenbug: verbose UART can mask the race.

### References

| Document | Purpose |
|----------|---------|
| [`BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) | Canonical fix recipe (from ultramegausb-atari-st-rpikbd **v22.1.0**) |
| [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) | Gap analysis, file map, test matrix, prior experiments on this repo |
| [`gamepad-support.md`](gamepad-support.md) | Gamepad Phase C and pairing cross-link |

### What we already have

- `flash_safe_execute_core_init()` on Core 1 (`quokkadb.cpp`)
- Partial Core 1 USB pause via `bt_host_coop` (gamepad discovery, Xbox/Stadia connect)
- `FlashSettings` sector below BTstack TLV (`flashsettings.cpp`)

### What to port (from Atari handoff)

- [ ] Refcounted Core 1 pause (replace single bool in `bt_host_coop.c`)
- [ ] Core 1 pause loop: `__wfe()` instead of `busy_wait_us` spin
- [ ] `bt_callback_busy_wait_ms()` — **no `sleep_ms` in Bluepad32 callbacks**
- [ ] 30 ms settle after pause on gamepad discovery
- [ ] 100 ms busy-wait before resume in `on_device_ready`
- [ ] `core1_wait_for_pause_active()` before BTstack flash activity
- [ ] No double-pause on `device_connected` if already paused on discovery
- [ ] Resume on disconnect if `pause_depth > 0`
- [ ] Tunable constants in `bt_pairing_config.h` (or similar)
- [ ] Optional phase / `pause_depth` diagnostics (gate behind build flag)
- [ ] Hardware test matrix on Pico W and Pico 2 W (**release** build, minimal UART)

### Do not repeat without cause

Reverted on `feature/joysticks` with no improvement: shorter pairing delays (10 ms), `__not_in_flash_func` on Core 1 pause path, display off during pair. See [`changes.md`](changes.md).

### Open questions

- **125 MHz** system clock vs Atari’s **225 MHz** for CYW43 — evaluate only after full pairing port.
- Keyboard/mouse pairing without Core 1 pause — Atari handoff says short path; we still saw MX Keys hangs. Revisit if gamepad recipe alone is insufficient.

---

## 2. Gamepad — native ADB / Gravis (Phase C)

**Priority:** Medium (after pairing stability)  
**Status:** Phase A+B shipped (BT gamepad → keyboard+mouse emulation); Phase C planned  
**Reference:** [`gamepad-support.md`](gamepad-support.md), [`gravis_mousestick_ii.md`](gravis_mousestick_ii.md)

- [ ] Detect ADB handler switch to **0x23**; implement Talk 0 / Talk 1 per Gravis docs
- [ ] Map BT gamepad axes/buttons to native ADB joystick reports
- [ ] USB HID gamepads via TinyUSB → same bridge as Bluetooth
- [ ] User-toggle modes + persisted keymap (`FlashSettings` — re-verify flash layout)

---

## 3. UI / OLED alignment

**Priority:** Medium  
**Status:** In progress on `feature/ui-alignment` (splash / devices / map devices per ULTRAMEGAUSB spec)

- [ ] Finish and merge UI alignment branch
- [ ] Verify on hardware (Pico W / Pico 2 W with buttons + display)

---

## 4. Joystick / Gravis Flightstick (broader)

**Priority:** Lower  
**Reference:** [`todo.md`](todo.md) §1

USB + BLE joystick support, normalized event model, mapping profiles, rate limiting. Depends on stable BT pairing and gamepad foundation.

---

## 5. adbmon-pico + diagnostic board

**Priority:** Lower (high leverage for debugging)  
**Reference:** [`todo.md`](todo.md) §2

Pico-native ADB bus monitor, timing capture, diagnostic interposer PCB. Useful for IIgs timing work and future tablet/joystick bring-up.

---

## 6. USB tablet → ADB Wacom

**Priority:** Lower  
**Reference:** [`todo.md`](todo.md) §3, [`wacom.md`](wacom.md)

Absolute positioning, pressure, Wacom-style ADB register packing.

---

## Suggested order

1. **Bluetooth pairing stability** — unblocks reliable multi-device BT (KB + mouse + gamepad).
2. **Gamepad Phase C** — native Gravis/ADB once pairing is solid.
3. **UI alignment** — merge when ready; independent of pairing but benefits from stable BT counts/names.
4. **adbmon-pico** — when deep ADB/trace debugging is needed.
5. Joystick / Wacom — larger features after core HID paths are stable.
