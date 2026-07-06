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

**See also §11** for full MouseStick II emulation with modern gamepads and Gravis client software (likely depends on §10 ADBMON).

---

## 3. UI / OLED alignment

**Priority:** Medium  
**Status:** Implemented on `feature/ui-alignment` (splash / devices / map devices per ULTRAMEGAUSB spec)

- [x] Three-screen flow, USB map, L+R pairing clear
- [ ] Merge branch; verify on hardware

---

## 7. ADB passthrough hub — Phase 2 (active repeater)

**Priority:** Medium  
**Status:** Phase 1 done — relocated addresses; see [`adb-passthrough-hub.md`](adb-passthrough-hub.md)

- [x] Hub mode: spec enumeration (defaults **0x02/0x03**, host Listen **0xFE**, collision); OLED footer
- [ ] OLED / button toggle for hub mode and custom addresses
- [ ] PIO bit-level repeater if hardware uses split host/device segments
- [ ] Optional proxy: forward Talk to downstream when adapter is passive listener

---

## 4. Joystick / Gravis Flightstick (broader)

**Priority:** Lower  
**Reference:** [`todo.md`](todo.md) §1

USB + BLE joystick support, normalized event model, mapping profiles, rate limiting. Depends on stable BT pairing and gamepad foundation.

---

## 5. ADBMON — ADB bus monitor (on-adapter + diagnostic hardware)

**Priority:** Medium (high leverage for hub, Gravis, IIgs timing)  
**Status:** **Partial — standalone firmware shipped 2026-06-28** — see [`src/adbmon/README.md`](../src/adbmon/README.md), [`todo.md`](todo.md) §2

### Use case

Enable the adapter (or a dedicated monitor build) to **debug traffic on the ADB bus** — essential for hub handoff, enumeration, SRQ behaviour, and reverse-engineering protocols (e.g. Gravis handler **0x23**).

### Output paths

| Path | Idea |
|------|------|
| **Serial (UART / USB CDC)** | Full decoded trace: Attention, command byte (addr / Talk·Listen / register), payload, SRQ extension, timing (µs). Ring-buffered so logging does not perturb bit timing. |
| **OLED** | Compact live view: last command, addresses, SRQ/collision flags, rolling hex — for bench use without a host PC. Optional dedicated “monitor” screen in the UI flow. |

### Firmware tasks (adapter-integrated or `adbmon` build target)

- [x] Passive **DATA line** capture (GPIO snoop) with timestamps — **2026-06-28** (`src/adbmon`)
- [x] Decode frames: Attention, command byte, Talk/Listen payload, SRQ, global reset — **2026-06-28**
- [x] **Monitor-only** standalone UF2 — does not emulate keyboard/mouse — **2026-06-28** (`./build-all.sh` → `dist/adbmon-pico.uf2`)
- [x] Serial (UART + USB CDC) trace output — **2026-06-28**
- [ ] Modes: raw edge dump, strict vs lenient timing toggles (partial — CMake flags exist)
- [ ] Multi-byte Talk payloads (Gravis 7-byte register 0)
- [ ] OLED live view (shared UI with main firmware)
- [ ] Adapter-integrated monitor mode (runtime toggle)
- [ ] Export snippets for regression fixtures (compare traces across firmware versions)

### Diagnostic board (optional hardware)

- [ ] Inline interposer PCB (ADB in/out), high-impedance monitor tap, activity LEDs — see [`todo.md`](todo.md) §2
- [ ] Validate capture does not alter host/device behaviour

### Unblocks

- [`adb-passthrough-hub.md`](adb-passthrough-hub.md) — SRQ handoff, address relocation verification
- **§9** intelligent mouse SRQ — observe when host Talks which address
- **§11** Gravis MouseStick II — capture real stick traffic for Talk 0/1 reference

---

## 6. USB tablet → ADB Wacom

**Priority:** Lower  
**Reference:** [`todo.md`](todo.md) §3, [`wacom.md`](wacom.md)

Absolute positioning, pressure, Wacom-style ADB register packing.

---

## 8. Multiple ADB pointing devices (separate cursors)

**Priority:** Lower  
**Status:** **Partial — host mode multi-device polling shipped 2026-06-28**; device-mode USB/BT merge unchanged

### Current behaviour

- **ADB → Mac (device mode):** one `mouse_addr` on the bus; `bt_hid_bridge` sums deltas from up to two BT mice plus gamepad stick into a single `MousePrs` stream.
- **ADB → USB (host mode):** keyboard @0x2 plus up to three pointing devices (@0x3, @0xE, @0xF) with address-aware classification and hot-plug rescan — **2026-06-28**.
- Chained physical ADB devices (trackball, Gravis stick) are separate bus participants with their own addresses after host enumeration.

### Future option (device mode / multi-cursor on Mac)

- [ ] Expose **second (and third) ADB mouse instances** at unique addresses (e.g. **0x03**, **0x04**, **0x06**) for multi-cursor scenarios on classic Mac / IIgs
- [ ] Per-device routing: USB port / BT slot / gamepad stick → dedicated ADB address
- [ ] Hub mode: coordinate with downstream **0x03** trackball so adapter USB/BT mice relocate without stealing the downstream default
- [ ] OLED map screen: show which logical device maps to which ADB address
- [ ] Mac OS limits: document which systems support multiple pointing devices vs a single “primary” mouse

---

## 9. Intelligent mouse SRQ (hub + chained trackball)

**Priority:** Medium  
**Status:** Open — hub mouse SRQ experiments **reverted** (movement hitches); default remains keyboard SRQ only. See [`adb-passthrough-hub.md`](adb-passthrough-hub.md) and [`iigs-debugging.md`](iigs-debugging.md) §10.

### Problem

`ADB_IIGS_MOUSE_SUPPRESS_SRQ` (default **ON**) stops the adapter from asserting **mouse SRQ** on the ADB bus. That avoids IIgs BASIC slowdown when USB/BT mice move, but with a **chained trackball** on **0x03**:

- While the trackball moves, the host polls **0x03** often.
- When the trackball goes idle, the host may **stop polling 0x03** if neither the trackball nor the adapter SRQs.
- USB/BT mouse movement and left click sit in `mousepending` with no bus wake-up until **keyboard SRQ** (e.g. a key, or right-click in ctrl-click mode which enqueues Ctrl).

### Interim behaviour

- **Hub mode OFF:** unchanged — keyboard SRQ only (IIgs-friendly).
- **Hub mode ON:** mouse SRQ **not** asserted on the wire (same as non-hub; interim hub SRQ experiments reverted — caused movement hitches). USB/BT mouse may appear idle after trackball use until keyboard SRQ wakes the bus; see §9.

### Future: smarter detection (not just hub toggle)

- [ ] Detect when a **downstream pointing device** is present at the shared/default mouse address (trackball SRQ / Talk **0xC** activity / register-3 collision history) and enable mouse SRQ only in that configuration — not for every hub-mode user who might still want IIgs BASIC performance.
- [ ] Optional **runtime policy**: suppress / auto / always for mouse SRQ (OLED or flash setting), independent of full hub mode.
- [ ] Consider **piggyback** strategies (e.g. brief keyboard SRQ co-assert) vs continuous mouse SRQ if BASIC slowdown returns on IIgs + trackball setups.
- [ ] Document Mac vs IIgs expectations in [`adb-passthrough-hub.md`](adb-passthrough-hub.md).

---

## 10. ADB Host mode (ADB accessories → USB host)

**Priority:** Medium  
**Status:** **Complete (MVP) — 2026-06-28** — firmware **2.0.0** + GPIO split **2.1.0** + host reliability **2.2.0**; see [`adb-host-mode.md`](adb-host-mode.md), [`troubleshooting.md`](troubleshooting.md)

### Use case

Plug **real ADB accessories** (keyboard, mouse, trackball) into the adapter and present them to a **USB host** (Mac/PC/Pi) as standard USB HID — e.g. use a vintage ADB keyboard on a modern machine.

**Mode selection is manual:** user picks **ADB → Mac** (default, today’s USB/BT bridge) or **ADB → USB** on the OLED before changing wiring. See [`adb-host-mode.md`](adb-host-mode.md).

### Architecture sketch

| Today (device mode) | Host mode (proposed) |
|---------------------|----------------------|
| USB/BT → emulated ADB kbd/mouse | ADB Talk/Listen → USB HID reports |
| Core 1: TinyUSB **host** (input devices) | Core 1: TinyUSB **device** (to PC) |
| Core 0: ADB **device** GPIO bit-bang | Core 0: ADB **host** — issue Talk/Listen, enumerate bus |

Modes are **mutually exclusive** — user switches explicitly; one USB stack active at a time.

### Shipped (2026-06-28)

- [x] Manual OLED mode switch **ADB → Mac** / **ADB → USB**, persisted in flash
- [x] Unified **Pico 2 W** build (`ADB_HOST_MODE=ON`) with runtime toggle
- [x] ADB bus master: global reset, Talk R3 enumeration, keyboard + multi pointing-device poll
- [x] Mac-style trackball relocation; periodic hot-plug rescan (~3 s)
- [x] USB HID composite to PC; **ADB Bus** OLED screen (configured vs working)
- [x] Host timing: **765 µs** attention + RX preamble (`adb_host.cpp`)
- [x] Device/host GPIO split (`adb_host_gpio.h`) — collision fix **2.1.0**
- [x] **Manual mode switch** — no auto-detect from VBUS or ADB traffic (v1)
- [x] **Mutually exclusive USB roles** — cannot host USB-A peripherals and present HID to PC simultaneously on native USB

### Open questions / follow-ups

- [ ] Electrical / bus power policy in ADB host mode
- [ ] Hub hat support matrix in `ADB → USB` mode
- [ ] Coexistence with passthrough hub logic when Pico is bus master
- [ ] Optional: VBUS hint on mode screen only — not auto-switch

### References

- Apple enumeration / collision model — [`ADB_Overview`](https://en.wikipedia.org/wiki/Apple_Desktop_Bus) (see also project `docs/` ADB notes)
- [`adb-passthrough-hub.md`](adb-passthrough-hub.md) — shared-bus topology

---

## 11. Full Gravis MouseStick II emulation (modern gamepad + Gravis cdev)

**Priority:** Medium (after §2 Phase C foundations and ideally §5 ADBMON)  
**Status:** Not started — Phase B ships MouseStick-**like** handler **0x01** (stick → mouse + keys); native **0x23** not implemented  
**Reference:** [`gamepad-support.md`](gamepad-support.md) mode 2, [`gravis_mousestick_ii.md`](gravis_mousestick_ii.md), [`gravis-mousestick-ii-plan.md`](gravis-mousestick-ii-plan.md), [`adb_device_list.md`](adb_device_list.md)

### Use case

Use a **modern Bluetooth/USB gamepad** with **Gravis MouseStick II client software** on a vintage Mac (handler switch to **0x23**, Talk 0 seven-byte / three-byte reports, Talk 1 protocol id). Games and control panels that expect a real MouseStick II should see authentic register payloads, not keyboard emulation.

### Why ADBMON (§5) matters

Handler **0x23** behaviour is host-driven and payload-specific. Capturing **real MouseStick II** (or compatible) traffic on the bus — Talk 0/1 sequences, timing, button/axis encoding — de-risks emulation. **Sniff first, implement second.**

- [ ] Record reference traces with §5 ADBMON (real stick or known-good Mac + cdev)
- [ ] Document handler switch **0x01 → 0x23** and Listen R3 side effects
- [ ] Implement Talk **1** (`0x03 0x00` / `0x04 0x00` protocol ids)
- [ ] Implement Talk **0** 7-byte and 3-byte forms; map gamepad axes/buttons per captured reference
- [ ] USB HID gamepads → same native path as BT (TinyUSB host)
- [ ] Test matrix: Mac with Gravis cdev, titles from `adb_device_list.md`, IIgs where applicable
- [ ] OLED: show active handler (**0x01** vs **0x23**), not user-selected — host-driven only

### Relationship to §2

§2 **Phase C** is the umbrella (handler detection + Talk 0/1). §11 is the **product goal**: gamepad + Gravis software compatibility, with explicit dependency on bus capture for fidelity.

---

## Suggested order

1. **Bluetooth pairing stability** — unblocks reliable multi-device BT (KB + mouse + gamepad).
2. **ADBMON (§5)** — extend capture (Gravis payloads, OLED, adapter-integrated toggle).
3. **UI alignment** — merge when ready; independent of pairing but benefits from stable BT counts/names.
4. **Intelligent mouse SRQ (§9)** — refine hub/trackball detection; use ADBMON traces to validate.
5. **Full Gravis MouseStick II (§11)** — after ADBMON reference captures; extends §2 Phase C.
6. **Device-mode multi-cursor (§8)** — separate USB/BT mice to distinct ADB addresses on vintage Mac.
7. Joystick / Wacom — larger features after core HID paths are stable.

~~6. **ADB Host mode (§10)** — shipped 2026-06-28.~~
