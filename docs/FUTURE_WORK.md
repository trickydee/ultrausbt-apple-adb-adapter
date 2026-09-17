# Future work

Roadmap for **ultrausbt-apple-adb-adapter**. Changelog: [`release-notes.md`](release-notes.md). Archived backlog notes: [`archive/todo.md`](archive/todo.md).

---

## Done (keep for context)

| Item | Version | Notes |
|------|---------|--------|
| Bluetooth pairing stability (Atari/Amiga recipe + Apple extras) | **2.2.1** | User guide: [`bluetooth-pairing.md`](bluetooth-pairing.md); history: [`archive/`](archive/) |
| ADB Host mode MVP + GPIO split + reliability | **2.0.0–2.2.0** | Guide: [`adb-host-mode.md`](adb-host-mode.md) |
| ADB Host fast typing (HID queue / 8 ms poll) | **2.2.3** | |
| OLED Device/Host labels + `*` toggle | **2.2.3** era | Splash `ADB Dev` / `ADB Host` |
| Standalone adbmon UF2 | **2026-06-28** | [`src/adbmon/README.md`](../src/adbmon/README.md) |
| Gamepad Phase A+B (BT → keys + mouse) | — | [`gamepad-support.md`](gamepad-support.md) |

Optional follow-ups (not blocking): release-UF2 retest matrix (Pico W + Pico 2 W; Mac cold boot + Xbox-first); `pause_depth` diagnostics behind `ADB_DEBUG`; **125 MHz vs 225 MHz** CYW43 clock experiment if hangs return.

---

## 1. Gamepad — native ADB / Gravis (Phase C)

**Priority:** Medium  
**Status:** Phase A+B shipped; Phase C planned  
**Reference:** [`gamepad-support.md`](gamepad-support.md), [`gravis_mousestick_ii.md`](gravis_mousestick_ii.md), [`gravis-mousestick-ii-plan.md`](gravis-mousestick-ii-plan.md)

- [ ] Detect ADB handler switch to **0x23**; Talk 0 / Talk 1 per Gravis docs
- [ ] Map BT gamepad axes/buttons to native ADB joystick reports
- [ ] USB HID gamepads → same bridge as Bluetooth
- [ ] User-toggle modes + persisted keymap (`FlashSettings`)

Full product goal (gamepad + Gravis cdev): **§6**. Prefer adbmon captures of a real MouseStick II first (**§3**).

---

## 2. ADB passthrough hub — Phase 2

**Priority:** Medium  
**Status:** Phase 1 done (relocated addresses) — [`adb-passthrough-hub.md`](adb-passthrough-hub.md)

- [ ] OLED / button toggle for hub mode and custom addresses
- [ ] PIO bit-level repeater if hardware uses split host/device segments
- [ ] Optional proxy: forward Talk to downstream when adapter is passive listener

---

## 3. ADBMON — extend monitor + optional hardware

**Priority:** Medium (high leverage for hub, Gravis, IIgs)  
**Status:** Standalone firmware shipped — extend capture / UX

- [ ] Modes: raw edge dump; stricter timing toggles
- [ ] Multi-byte Talk payloads (Gravis 7-byte R0)
- [ ] OLED live view; adapter-integrated monitor toggle
- [ ] Export snippets into [`fixtures/`](fixtures/) for regression
- [ ] Optional inline interposer PCB (high-Z tap, activity LEDs)

Captures today: [`fixtures/adb-host/`](fixtures/adb-host/). Checklist: [`adb-host-mode-capture.md`](adb-host-mode-capture.md).

---

## 4. Intelligent mouse SRQ (hub + chained trackball)

**Priority:** Medium  
**Status:** Open — hub mouse-SRQ experiments reverted; default remains keyboard SRQ only  
**Reference:** [`adb-passthrough-hub.md`](adb-passthrough-hub.md), [`iigs-debugging.md`](iigs-debugging.md)

- [ ] Enable mouse SRQ only when a downstream pointer is detected (not blanket hub-on)
- [ ] Runtime policy: suppress / auto / always (OLED or flash)
- [ ] Document Mac vs IIgs expectations in the hub doc

---

## 5. Device-mode multi-cursor (separate ADB mice)

**Priority:** Lower  
**Status:** Host mode already polls multiple pointers; device mode still one `mouse_addr`

- [ ] Second/third ADB mouse instances at unique addresses
- [ ] Per-device routing USB/BT → ADB address; OLED map
- [ ] Document which Mac OS versions support multiple pointers

---

## 6. Full Gravis MouseStick II (gamepad + Gravis cdev)

**Priority:** Medium (after §1 / §3)  
**Status:** Not started — Phase B is handler **0x01** only  
**Reference:** [`gravis-mousestick-ii-plan.md`](gravis-mousestick-ii-plan.md), [`adb_device_list.md`](adb_device_list.md)

- [ ] Capture real stick (or known-good Mac + cdev) with adbmon
- [ ] Handler **0x01 → 0x23**; Talk 1 protocol ids; Talk 0 7-/3-byte forms
- [ ] USB + BT gamepads on the same native path; OLED shows active handler

---

## 7. Joystick / Gravis Flightstick (broader)

**Priority:** Lower  

USB + BLE joystick support, normalized event model, mapping profiles, rate limiting. Builds on gamepad foundation.

---

## 8. USB tablet → ADB Wacom

**Priority:** Lower  
**Reference:** [`fixtures/wacom.md`](fixtures/wacom.md)

Absolute positioning / pressure; Wacom-style ADB register packing. Large feature — after relative mouse path stays stable.

---

## 9. ADB Host follow-ups

**Priority:** Lower  
**Guide:** [`adb-host-mode.md`](adb-host-mode.md)

- [ ] Bus power policy documentation
- [ ] Hub-hat behaviour when Pico is bus master
- [ ] Coexistence with passthrough hub logic
- [ ] Optional VBUS hint on Mode screen (not auto-switch)

---

## Suggested order

1. Extend **ADBMON (§3)** — Gravis payloads, fixtures, optional OLED  
2. **Intelligent mouse SRQ (§4)** — hub/trackball wake  
3. **Gravis MouseStick II (§6)** after real-stick captures  
4. Hub Phase 2 / multi-cursor / joystick / Wacom as capacity allows  
