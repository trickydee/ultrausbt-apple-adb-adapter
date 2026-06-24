# Bluetooth pairing — Apple ADB adapter port notes

**Status:** Work not started (reference docs only). Tracked in [`FUTURE_WORK.md`](FUTURE_WORK.md) §1.  
**Canonical fix recipe:** [`BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) (from ultramegausb-atari-st-rpikbd v22.1.0).  
**Related:** [`gamepad-support.md`](gamepad-support.md), [`changes.md`](changes.md) (session notes).

This document maps the Atari handoff to **ultramegausb-apple-adb** firmware: what we already have, what is missing, prior experiments, and the hardware test matrix to run after porting.

---

## Symptom history on this project

- **Intermittent lockups when adding a Bluetooth device** (keyboard, mouse, or gamepad) on Pico W / Pico 2 W.
- Example capture: **Logitech MX Keys M** — discovery → `SM_EVENT_PAIRING_STARTED` → `Connection encrypted: 1` → stall. “Identity resolving failed” in log is often non-fatal.
- **v1.0.8-era fixes** (flash-safe init + Core 1 pause) were a **clear improvement** but did **not** eliminate random hangs.
- Hangs were **Heisenbugs**: verbose `printf` / `logi` sometimes made them disappear or move.
- **Second-device** pairing (KB + mouse already connected, then gamepad) was a common failure mode.

---

## Architecture mapping (Atari → Apple ADB)

| Atari ST IKBD | Apple ADB adapter |
|---------------|-------------------|
| Core 0: TinyUSB, Bluepad32, OLED | Core **0**: ADB, `bluepad32_poll()`, OLED, `process_bluepad32_devices()` |
| Core 1: `hd6301_run_clocks()` from **XIP** | Core **1**: `tuh_task()`, `ChangeUSBKeyboardLEDs()` from **XIP** |
| `g_core1_pause_depth` + `__wfe()` in pause loop | `bt_host_coop` — **single `bool`**, `busy_wait_us(5000)` spin |
| `bt_callback_busy_wait_ms()` in platform callbacks | **`sleep_ms()`** in `bluepad32_platform.c` |
| `NVSettings.cpp` sector layout | `flashsettings.cpp` → `settings_flash_offset_bytes()` |
| System clock **225 MHz** (BT builds) | System clock **125 MHz** (`set_sys_clock_khz(125000)` in `quokkadb.cpp`) |

The **multicore flash race** is the same: BTstack on Core 0 writes pairing TLV via `flash_safe_execute()` while Core 1 must stop executing from XIP flash.

---

## Handoff checklist — current firmware status

| Item | Status | Location / notes |
|------|--------|------------------|
| `flash_safe_execute_core_init()` on Core 1 | **Done** | `lib/QuokkADB/src/quokkadb.cpp` → `core1_main()` |
| Core 1 pause loop uses `__wfe()` | **Missing** | Uses `busy_wait_us(5000)` + `continue` |
| Refcounted Core 1 pause | **Missing** | `src/bt_host_coop.c` — single volatile bool |
| Pause on gamepad discovery (CoD 0x0508 / name) | **Partial** | `bluepad32_platform.c` → `my_platform_on_device_discovered()` |
| No double-pause on `device_connected` | **Gap** | Xbox/Stadia pause on connect **after** gamepad discovery may already have paused |
| 30 ms settle after pause | **Missing** | No `BT_GAMEPAD_DISCOVERY_SETTLE_MS` equivalent |
| 100 ms busy-wait before resume in `device_ready` | **Missing** | Gamepad path uses `sleep_ms(10)` or `sleep_ms(50)` only |
| `core1_wait_for_pause_active()` | **Missing** | — |
| BT callbacks: `busy_wait_us` only | **Violated** | `sleep_ms(2000)` in `on_init_complete`; `sleep_ms(10/50)` in `on_device_ready` |
| Resume on disconnect if pairing aborted | **Partial** | `set_paused(false)` on disconnect; no depth tracking |
| NV / settings flash below BTstack TLV | **Done** | `lib/QuokkADB/src/flashsettings.cpp` |
| SDK-pinned BTstack | **Yes** | `pico_btstack_*` via Bluepad32 CMake |
| Avoid 2 ms HID / duplicate handlers without retest | **N/A yet** | Single `process_bluepad32_devices()` per loop today |
| 225 MHz for CYW43 (Atari) | **Different** | We use 125 MHz; evaluate separately if hangs persist after full port |

---

## Code map (this repo — files to change when porting)

| File | Responsibility today | Port action |
|------|----------------------|-------------|
| `lib/QuokkADB/src/quokkadb.cpp` | Core 1 USB loop, `flash_safe_execute_core_init` | Pause loop → `__wfe()`; optional phase/pause diagnostics |
| `src/bt_host_coop.c` / `include/bt_host_coop.h` | Single bool USB-host pause flag | Refcount; `wait_for_pause_active`; rename API to match Atari pattern if desired |
| `src/bluepad32_platform.c` | Discovery/connect/ready hooks, `sleep_ms` delays | `bt_callback_busy_wait_ms`; settle + resume constants; remove connect double-pause; resume all device types if paused |
| `src/bluepad32_init.c` | CYW43 power-cycle sleeps | Review: init sleeps may be OK outside pairing callbacks |
| `lib/QuokkADB/src/flashsettings.cpp` | Settings sector below TLV | **No change expected** — already fixed for RP2040/RP2350 A2 |
| New header (suggested) | — | `src/bt_pairing_config.h` or constants in `display_config.h`: `BT_GAMEPAD_DISCOVERY_SETTLE_MS`, `BT_GAMEPAD_CORE1_RESUME_DELAY_MS` |

---

## Prior experiments on this repo (do not repeat without cause)

Documented in [`changes.md`](changes.md) — all **reverted**, **no pairing improvement** observed:

1. Shortening pairing delays (50 ms / 200 ms → 10 ms).
2. `__not_in_flash_func` on the Core 1 pause path.
3. Optional display off via `ENABLE_DISPLAY_UPDATE` in `display_config.h`.

Earlier branches also tried **pausing Core 1 for all HID** devices (not just gamepads); narrowed to gamepad/Xbox/Stadia only. Keyboard-only hangs (MX Keys) may need revisiting after the full Atari recipe is ported — the handoff assumes KB/mouse do not need pause, but we saw keyboard pairing stalls before gamepad work landed.

---

## Suggested porting order

Follow [`BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) §Suggested porting order. For this repo specifically:

1. **`bt_host_coop`** — refcount + `core1_wait_for_pause_active()` (port from Atari `main.cpp`).
2. **`quokkadb.cpp`** — Core 1 pause branch: `__wfe()` instead of busy-spin.
3. **`bluepad32_platform.c`** — `bt_callback_busy_wait_ms()`; discovery settle 30 ms; ready resume delay 100 ms; pause once on discovery only; resume on ready **and** disconnect if `depth > 0`.
4. Add **`bt_pairing_config.h`** with tunable constants (do not hide delays inside `logi` paths).
5. **Hardware matrix** (release build, minimal UART).
6. Only then: Core 0 loop timing, clock speed experiments, BTstack upgrade.

---

## Hardware test matrix

Run on **Pico W** and **Pico 2 W**, **release** firmware (verbose UART off — it masks races).

| # | Steps | Pass criteria |
|---|--------|----------------|
| 1 | Fresh flash → pair BT keyboard only | Connects, types on ADB host, survives reboot |
| 2 | Add BT mouse (keyboard still connected) | Both work; no hang during mouse pair |
| 3 | With KB + mouse connected → pair **Stadia or Xbox BLE gamepad** | Gamepad pairs; KB/mouse still work; OLED responsive |
| 4 | Reboot → all three reconnect | No hang on autoconnect |
| 5 | Map Devices screen → **L+R 5 s** clear pairings → re-pair all three | Clean pair cycle |
| 6 | USB keyboard + mouse plugged **and** BT gamepad paired | No regression on USB or BT paths |

**Failure notes to capture:** last log line before hang; whether OLED frozen (Core 0 blocked) or only input dead; `pause_depth` if diagnostics added.

---

## Open questions

1. **125 MHz vs 225 MHz** — Atari uses 225 MHz for CYW43 on BT builds; we use 125 MHz (QuokkADB heritage). Port pairing fixes first; treat clock as a separate experiment.
2. **Keyboard/mouse pairing without Core 1 pause** — Atari handoff says short path; we still saw MX Keys hangs. If gamepad recipe is ported and KB hangs remain, consider selective pause for BLE HID CoD `0x05xx` or post-pause settle on `device_connected` for keyboards only.
3. **Display I2C on Core 0** — ruled out as root cause on Atari (Map Devices / `usb_device_map`). Our OLED still runs every Core 0 loop; unlikely primary cause but avoid heavy redraw during pair tests.

---

## LLM session paste (Apple ADB context)

> Port Atari v22.1.0 BT pairing fix into ultramegausb-apple-adb: see `docs/BT_PAIRING_HANDOFF.md` + `docs/BT_PAIRING_APPLE_ADB.md`. Core 0 = ADB/BT/OLED; Core 1 = TinyUSB from XIP. Already have `flash_safe_execute_core_init()` and `settings_flash_offset_bytes()`. Missing: refcounted pause, `__wfe()` on Core 1, `busy_wait_us` only in BT callbacks, 30 ms post-pause settle, 100 ms pre-resume delay, no double-pause on connect. Prior delay/display experiments reverted. Test KB + mouse + gamepad on Pico W and Pico 2 W without verbose UART.
