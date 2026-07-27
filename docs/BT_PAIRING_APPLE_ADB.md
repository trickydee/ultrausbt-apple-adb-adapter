# Bluetooth pairing — Apple ADB adapter port notes

**Status:** **Done** — shipped in firmware **2.2.1**.
**Canonical fix recipe:** [`BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) (from ultramegausb-atari-st-rpikbd v22.1.0).  
**Sibling port (done):** [`BT_PAIRING_PORT_AMIGA_REFERENCE.md`](BT_PAIRING_PORT_AMIGA_REFERENCE.md) — ultramegausb-amiga `feature/BT-Pairing-align` (v2.2.11).  
**Related:** [`gamepad-support.md`](gamepad-support.md), [`bluetooth-pairing.md`](bluetooth-pairing.md) (user guide), [`troubleshooting.md`](troubleshooting.md), [`changes.md`](changes.md).

This document maps the Atari handoff to **ultramegausb-apple-adb** firmware: what was ported, Apple-specific fixes beyond the Atari recipe, prior experiments, and the hardware test matrix.

---

## Symptom history on this project

- **Intermittent lockups when adding a Bluetooth device** (keyboard, mouse, or gamepad) on Pico W / Pico 2 W.
- Example capture: **Logitech MX Keys M** — discovery → `SM_EVENT_PAIRING_STARTED` → `Connection encrypted: 1` → stall. “Identity resolving failed” in log is often non-fatal.
- **v1.0.8-era fixes** (flash-safe init + Core 1 pause) were a **clear improvement** but did **not** eliminate random hangs.
- Hangs were **Heisenbugs**: verbose `printf` / `logi` sometimes made them disappear or move.
- **Second-device** pairing (KB + mouse already connected, then gamepad) was a common failure mode.
- **Mac cold boot + Xbox-first pairing order** could leave the BT mouse dead on ADB even when all three devices showed “device ready” in UART — caused by Mac global ADB reset colliding with BT enumeration (fixed by deferral below).

---

## Architecture mapping (Atari → Apple ADB)

| Atari ST IKBD | Apple ADB adapter |
|---------------|-------------------|
| Core 0: TinyUSB, Bluepad32, OLED | Core **0**: ADB, `bluepad32_poll()`, OLED, `process_bluepad32_devices()` |
| Core 1: `hd6301_run_clocks()` from **XIP** | Core **1**: `tuh_task()`, `ChangeUSBKeyboardLEDs()` from **XIP** |
| `g_core1_pause_depth` + `__wfe()` in pause loop | `bt_host_coop.c` — **refcounted depth** + `__wfe()` in pause loop |
| `bt_callback_busy_wait_ms()` in platform callbacks | `bt_callback_busy_wait_ms()` in `bluepad32_platform.c` — **no `sleep_ms` in callbacks** |
| `NVSettings.cpp` sector layout | `flashsettings.cpp` → `settings_flash_offset_bytes()` |
| System clock **225 MHz** (BT builds) | System clock **125 MHz** (`set_sys_clock_khz(125000)` in `quokkadb.cpp`) |

The **multicore flash race** is the same: BTstack on Core 0 writes pairing TLV via `flash_safe_execute()` while Core 1 must stop executing from XIP flash.

---

## Handoff checklist — current firmware status

| Item | Status | Location / notes |
|------|--------|------------------|
| `flash_safe_execute_core_init()` on Core 1 | **Done** | `lib/QuokkADB/src/quokkadb.cpp` → `core1_main()` |
| Core 1 pause loop uses `__wfe()` | **Done** | `quokkadb.cpp` — `g_core1_pause_spins++`; `__wfe()` |
| Refcounted Core 1 pause | **Done** | `src/bt_host_coop.c` / `include/bt_host_coop.h` |
| Pause on gamepad discovery (CoD 0x0508 / Stadia/Xbox name) | **Done** | `bluepad32_platform.c` → `discovery_needs_core1_pause()` |
| No double-pause on `device_connected` | **Done** | `on_device_connected` is empty |
| 30 ms settle after pause | **Done** | `BT_GAMEPAD_DISCOVERY_SETTLE_MS` in `bt_pairing_config.h` |
| 100 ms busy-wait before resume in `device_ready` | **Done** | `BT_GAMEPAD_CORE1_RESUME_DELAY_MS`; all device types if `depth > 0` |
| `core1_wait_for_pause_active()` | **Done** | `bt_host_coop.c` |
| BT callbacks: `busy_wait_us` only | **Done** | `bt_callback_busy_wait_ms()` — no `sleep_ms` in pairing callbacks |
| Resume on disconnect if pairing aborted | **Done** | `core1_resume_after_bt_enumeration()` if `depth > 0`; `core1_force_release_bt_pause()` on key wipe |
| 45 s pause watchdog | **Done** | `BT_CORE1_PAUSE_WATCHDOG_MS`; ticked from Core 0 loop |
| NV / settings flash below BTstack TLV | **Done** | `lib/QuokkADB/src/flashsettings.cpp` |
| SDK-pinned BTstack | **Yes** | `pico_btstack_*` via Bluepad32 CMake |
| Tunable constants header | **Done** | `include/bt_pairing_config.h` |
| 225 MHz for CYW43 (Atari) | **Different** | We use 125 MHz; evaluate separately if hangs persist |

---

## Apple ADB extensions (beyond Atari/Amiga recipe)

These fixes were required after the base port; they are **not** in the Atari handoff doc.

| Fix | Files | Why |
|-----|--------|-----|
| Always-merge BT keyboard + gamepad before `Parse()` | `bt_hid_bridge.cpp`, `bluepad32_peek_keyboard()` / `bluepad32_peek_gamepad()` | Xbox paired before keyboard caused spurious KeyUp/KeyDown via shared `prevState` at fake BT addr `0x80` → keyboard queue flood (`unable to enqueue new KeyDown`) |
| Suppress gamepad→keyboard keys during Core 1 pause when BT keyboard connected | `bt_hid_bridge.cpp` → `include_gamepad_keys_in_keyboard_report()` | Avoid phantom keys while Xbox BLE bond completes |
| Defer Mac global ADB reset during BT link setup + post-ready settle | `bluepad32_bt_defer_adb_reset()`, `quokkadb.cpp`, `BT_POST_READY_ADB_SETTLE_MS` (2500 ms) | Mac cold boot sends `ALL: Resetting devices` mid-enumeration → mouse dead on ADB despite successful BT `device ready` |
| Force-release pause on disconnect; clear orphan slots on failed connect | `bluepad32_platform.c`, `core1_force_release_bt_pause()` | Xbox sleep/wake reconnect failures; stuck `pause_depth` after aborted pair |
| Queue overflow warnings gated behind debug build | `usbkbdparser.cpp` | Reduce UART noise on release UF2 |

---

## Code map (this repo)

| File | Responsibility |
|------|----------------|
| `lib/QuokkADB/src/quokkadb.cpp` | Core 1 USB loop, `__wfe()` pause, deferred ADB reset apply, watchdog tick |
| `src/bt_host_coop.c` / `include/bt_host_coop.h` | Refcount pause API, watchdog, force release |
| `include/bt_pairing_config.h` | `BT_GAMEPAD_DISCOVERY_SETTLE_MS`, `BT_GAMEPAD_CORE1_RESUME_DELAY_MS`, `BT_CORE1_PAUSE_WATCHDOG_MS`, `BT_POST_READY_ADB_SETTLE_MS` |
| `src/bluepad32_platform.c` | Discovery/connect/ready/disconnect hooks; defer-ADB-reset state; peek APIs |
| `lib/QuokkADB/src/bt_hid_bridge.cpp` | Merged keyboard+gamepad reports; gamepad stick → mouse |
| `lib/QuokkADB/src/flashsettings.cpp` | Settings sector below TLV — no change needed |

---

## Prior experiments on this repo (do not repeat without cause)

Documented in [`changes.md`](changes.md) — all **reverted**, **no pairing improvement** observed:

1. Shortening pairing delays (50 ms / 200 ms → 10 ms).
2. `__not_in_flash_func` on the Core 1 pause path.
3. Optional display off via `ENABLE_DISPLAY_UPDATE` in `display_config.h`.

Earlier branches also tried **pausing Core 1 for all HID** devices (not just gamepads); narrowed to gamepad/Xbox/Stadia only. Keyboard-only hangs (MX Keys) may need revisiting if they return after this port — the handoff assumes KB/mouse do not need pause, but we saw keyboard pairing stalls before gamepad work landed.

---

## Recommended pairing order (hardware)

Use **release** UF2 (`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`), not debug, for pairing tests — verbose UART can mask timing races.

| Order | Mac cold boot | Mac already running |
|-------|---------------|---------------------|
| **Recommended** | Mouse → keyboard → gamepad (Xbox/Stadia) | Same; generally reliable |
| **Risky** | Gamepad (Xbox) → mouse → keyboard | May leave mouse dead until adapter reset (mitigated by defer-ADB-reset in 2.2.1+) |

**PS5 (DualSense)** uses BR/EDR and does **not** trigger the BLE gamepad Core 1 pause path — pairing is typically easier than Xbox BLE.

Clear stale bonds: **Map Devices** screen → hold **`˄+˯`** for 5 s.

---

## Hardware test matrix

Run on **Pico W** and **Pico 2 W**, **release** firmware (verbose UART off — it masks races).

| # | Steps | Pass criteria |
|---|--------|----------------|
| 1 | Fresh flash → pair BT keyboard only | Connects, types on ADB host, survives reboot |
| 2 | Add BT mouse (keyboard still connected) | Both work; no hang during mouse pair |
| 3 | With KB + mouse connected → pair **Stadia or Xbox BLE gamepad** | Gamepad pairs; KB/mouse still work; OLED responsive |
| 4 | Reboot → all three reconnect | No hang on autoconnect |
| 5 | Map Devices screen → **`˄+˯` 5 s** clear pairings → re-pair all three | Clean pair cycle |
| 6 | USB keyboard + mouse plugged **and** BT gamepad paired | No regression on USB or BT paths |
| 7 | **Mac cold boot** — pair mouse → keyboard → Xbox | All three work on ADB after Mac finishes boot |
| 8 | **Mac cold boot** — pair Xbox → mouse → keyboard | Mouse works on ADB (defer-ADB-reset); or document workaround if still flaky |
| 9 | Xbox sleep/wake → reconnect | Reconnects without stuck pause; USB/BT inputs recover |

**Failure notes to capture:** last log line before hang; whether OLED frozen (Core 0 blocked) or only input dead; `pause_depth` if using debug UF2.

---

## Open questions

1. **125 MHz vs 225 MHz** — Atari uses 225 MHz for CYW43 on BT builds; we use 125 MHz (QuokkADB heritage). Port is complete at 125 MHz; treat clock as a separate experiment if hangs persist.
2. **Keyboard/mouse pairing without Core 1 pause** — Atari handoff says short path; we still saw MX Keys hangs before this port. If KB-only stalls return, consider selective pause for BLE HID CoD `0x05xx`.
3. **Mac cold boot + Xbox-first** — defer-ADB-reset mitigates; retest on release 2.2.1+ before declaring fully closed.
4. **Display I2C on Core 0** — ruled out as root cause on Atari. Our OLED still runs every Core 0 loop; unlikely primary cause but avoid heavy redraw during pair tests.

---

## LLM session paste (Apple ADB context)

> **Context:** ultramegausb-apple-adb (Pico W / Pico 2 W). Core 0 = ADB + Bluepad32 + OLED. Core 1 = TinyUSB `tuh_task()` from XIP. **Port complete** on `feature/BT-alignment`: refcounted pause + `__wfe()` + `bt_callback_busy_wait_ms` + 30 ms settle + 100 ms pre-resume + no connect double-pause + watchdog. **Apple extras:** `peek_keyboard`/`peek_gamepad` always-merge in `bt_hid_bridge.cpp`; `bluepad32_bt_defer_adb_reset()` + 2.5 s post-ready settle; force-release pause on disconnect. **Read:** `docs/BT_PAIRING_HANDOFF.md`, this file, `docs/troubleshooting.md` § Bluetooth. **Test:** KB + mouse + Stadia/Xbox on Pico W and Pico 2 W with release UF2; Mac cold boot pair-order matrix.
