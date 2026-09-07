# Bluetooth pairing hangs — handoff notes (Pico / Bluepad32 / dual-core)

**Copied into ultrausbt-apple-adb** from `ultramegausb-atari-st-rpikbd/docs/BT_PAIRING_HANDOFF.md` (Atari ST IKBD emulator, fixes in **v22.1.0**). Keep in sync when the source doc changes.

**Apple-ADB porting status:** **Done** — shipped in firmware **2.2.1**. See [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) for port checklist and Apple-specific extensions; user guide: [`bluetooth-pairing.md`](bluetooth-pairing.md). Tracked in [`FUTURE_WORK.md`](FUTURE_WORK.md) §1.

---

**Audience:** LLM or developer working on another Pico 2 W HID host (e.g. Apple ADB adapter) that sees **random Bluetooth pairing hangs** while keyboards/mice work until a gamepad pairs.

**Source project:** ultramegausb-atari-st-rpikbd (Atari ST IKBD emulator). Fixes shipped in **v22.1.0** (`RELEASE_NOTES.md` §22.1.0). This doc distills what we saw and what we changed — patterns apply to any **dual-core firmware** where Core 0 runs USB/BT/UI and Core 1 runs a **tight loop from flash (XIP)**.

---

## Symptom pattern (what users reported)

- BLE **keyboard + mouse** work reliably.
- Pairing a **BLE HID gamepad** (Google Stadia, Xbox Wireless — CoD **0x0508**, HID-over-GATT) while KB/mouse are connected can:
  - **Hang the whole adapter** (OLED/UI frozen too → Core 0 blocked, not just Core 1).
  - Stop keyboard/mouse reaching the host protocol even though the UI still updates.
- **Intermittent / “random”** — classic race with flash and multicore timing.
- **PS5 DualSense (BLE)** often unaffected; problem clustered on **longer pairing paths** (Xbox/Stadia bonding + TLV flash writes).
- Bug **vanished or moved** when verbose `printf` / `logi` was added (Heisenbug).

---

## Architecture that makes this possible

| Piece | Role |
|-------|------|
| **Core 0** | TinyUSB, Bluepad32/BTstack, OLED, main loop |
| **Core 1** | Tight emulator loop (`hd6301_run_clocks()` from **XIP flash**) — no `sleep` in loop |
| **BTstack TLV** | Pairing keys / persistence written via **`flash_safe_execute()`** |
| **CYW43** | WiFi/BT chip; flash access must coordinate with **both cores** |

If Core 1 keeps executing from flash while Core 0/BTstack erases/writes flash, you get **stalls, corruption, or permanent freeze**. `flash_safe_execute()` tries to pause the other core — but only if that core cooperates.

---

## Root causes we confirmed

### 1. Core 1 not enrolled in flash-safe multicore

**Fix:** Call `flash_safe_execute_core_init()` at the **start of Core 1** before any XIP work.

Without this, BT pairing flash writes can freeze Core 1 or fail silently.

**Reference:** `src/main.cpp` → `core1_entry()`.

### 2. Core 1 still in XIP during pairing flash writes

**Fix:** **Pause Core 1** before BTstack writes pairing data, resume only after enumeration completes.

- Pause on **gamepad discovery** (CoD `0x0508` or name match `Stadia` / `Xbox`), not on every BLE device.
- **BLE keyboards/mice** use a shorter path — we do **not** pause Core 1 for them.
- Core 1 pause loop must **`__wfe()`** (not busy-spin forever) so multicore lockout / `flash_safe_execute` can preempt.

**Reference:** `src/main.cpp` (`g_core1_paused`, `CORE1_PHASE_PAUSED`), `src/bluepad32_platform.c` → `my_platform_on_device_discovered()`.

### 3. Single bool pause flag → refcount bugs

**Symptom:** Double-pause on `connected` + single resume on `ready` left **`pause_depth > 0`** forever, or resumed too early.

**Fix:** **Refcounted** `g_core1_pause_depth`:
- `core1_pause_for_bt_enumeration()` → `++depth`
- `core1_resume_after_bt_enumeration()` → `--depth` (only if depth > 0)
- Pause **once** on discovery; **do not pause again** in `on_device_connected`.
- Resume **once** in `on_device_ready` (or on disconnect if pairing aborted).

**Reference:** `src/main.cpp` — `core1_pause_for_bt_enumeration`, `core1_resume_after_bt_enumeration`.

### 4. `sleep_ms()` / `__wfe()` inside Bluepad32 callbacks froze Core 0

**Symptom:** Entire adapter hung including OLED — Core 0 stuck inside BT callback.

**Fix:** In BT platform callbacks use **`busy_wait_us()` only** via a small helper (`bt_callback_busy_wait_ms`). Never `sleep_ms()` or `__wfe()` on Core 0 in those paths.

**Reference:** `src/bluepad32_platform.c` — `bt_callback_busy_wait_ms()`.

### 5. Resume too early (race masked by debug logging)

**Symptom:** Production hang; debug build OK because extra `logi`/`printf` added ~10–50 ms delay.

**Fix:** Explicit delays in `include/config.h`:
- `BT_GAMEPAD_DISCOVERY_SETTLE_MS` = **30** — after pausing Core 1 on discovery, before BTstack flash activity.
- `BT_GAMEPAD_CORE1_RESUME_DELAY_MS` = **100** — in `on_device_ready`, before `core1_resume_after_bt_enumeration()`.

Also: `core1_wait_for_pause_active()` — poll until Core 1 reports `CORE1_PHASE_PAUSED` or `g_core1_pause_spins > 0` (busy-wait poll, max ~20 ms).

**Reference:** `src/bluepad32_platform.c` → `my_platform_on_device_ready()`.

### 6. NVSettings / custom flash overlapping BTstack TLV bank

Separate issue but same “random BT” class: **user flash sector must not overlap** the BTstack pairing TLV region. Board-aware sector layout in `src/NVSettings.cpp`.

### 7. Main-loop timing experiments broke pairing

Cherry-picking **2 ms** USB/HID polling (`handle_keyboard` twice per tick, etc.) caused **Stadia/Xbox pairing hangs**. Reverted to **10 ms** HID block + **1 ms** `bluepad32_poll()`.

Do not speed up Core 0 HID without retesting **multi-device BT pairing** (KB + mouse + gamepad).

**Reference:** `docs/FUTURE_WORK.md` — “Known regressions”; `AGENTS.md` — Core 0 timing table.

### 8. BTstack version pin

Upgrading BTstack past **pico-sdk v1.6.2** (`hids_client` → `hids_host` in v1.8+) broke compile or Xbox/Stadia classic paths. **Stay on SDK-pinned BTstack** unless you port `pico_btstack` CMake + Bluepad32 HID client.

**Reference:** `docs/FUTURE_WORK.md` — “BTstack upgrade experiment”.

---

## What did *not* cause the hang (ruled out)

- Map Devices OLED / `usb_device_map` / device name strings.
- Stadia vs Xbox being “classic BR/EDR only” on Pico 2 W — both were **BLE HID gamepads** in captures.
- Core 1 “frozen” with `pause_depth=0` — emulator stuck **inside** `hd6301_run_clocks` (XIP), not in pause spin; diagnose with phase/pc breadcrumbs.

---

## Checklist for a new Pico HID host (Apple ADB, etc.)

### Boot / multicore

- [ ] `flash_safe_execute_core_init()` on **secondary core** entry (first line of Core 1).
- [ ] Secondary core loop: when paused, **`__wfe()`** in pause branch.
- [ ] If secondary core runs from XIP: wireless build often needs **XIP** (RAM tight); USB-only may use `copy_to_ram`.

### Bluetooth platform callbacks

- [ ] **No** `sleep_ms()` / `__wfe()` on Core 0 in Bluepad32/BTstack callbacks.
- [ ] Delays: **`busy_wait_us()`** only.
- [ ] **Refcounted** Core 1 pause (not a single `bool`).
- [ ] Pause on **gamepad discovery** (or your long-pairing device class); resume on **`device_ready`** with **post-ready delay** before resume.
- [ ] On **disconnect during pairing**, resume if `pause_depth > 0`.
- [ ] Do **not** double-pause on `connected` if you already paused on `discovered`.

### Flash layout

- [ ] NVSettings / user config sector **below** BTstack TLV bank; board-specific addresses (Pico W vs Pico 2 W differ).

### Clock / RF

- [ ] BT builds: **225 MHz** system clock for CYW43 stability (270 MHz caused stalls in this project).
- [ ] USB-only builds: 270 MHz OK.

### Core 0 main loop

- [ ] Document and keep stable: HID handler call **frequency** and **once-per-tick** rules (duplicate `handle_keyboard()`-style bugs are unrelated to pairing but show how timing changes bite).
- [ ] `bluepad32_poll()` ~1 ms; avoid blocking Core 0 for tens of ms during pair.

### Diagnostics (build `BUILD_VARIANT=debug`)

Add before guessing:

| Signal | Meaning |
|--------|---------|
| `pause_depth` | Should return to **0** after `device_ready` |
| Core 1 `phase` | `PAUSED` during pair; `RUN_CLOCKS` when emulating |
| `pc` at `hd6301_run_clocks` entry | If frozen with `phase=RUN_CLOCKS` and cycles not advancing → stuck in emulator/flash |
| `[CYCLES_FROZEN!]` on heartbeat | Core 1 cycle counter unchanged 10 s |
| `BT(kb=… mouse=… joy=…)` | Device counts still updating when hang is “soft” |

Gate verbose `[DIAG]` logs behind `ENABLE_SERIAL_LOGGING` — they **change timing** and can hide races.

---

## Minimal code map (Atari project)

| File | Responsibility |
|------|----------------|
| `src/main.cpp` | Core 1 loop, pause/refcount, `flash_safe_execute_core_init`, heartbeat diagnostics |
| `src/bluepad32_platform.c` | Discovery pause, ready resume + delays, `busy_wait` only, storage getters |
| `include/config.h` | `BT_GAMEPAD_DISCOVERY_SETTLE_MS`, `BT_GAMEPAD_CORE1_RESUME_DELAY_MS`, `CYCLES_PER_LOOP` |
| `src/NVSettings.cpp` | Flash sector layout vs BTstack TLV |
| `CMakeLists.txt` | XIP for wireless, `copy_to_ram` for USB-only |

---

## Suggested porting order (Apple ADB or similar)

1. Confirm **dual-core + XIP on Core 1** — if yes, flash-safe path is mandatory.
2. Add `flash_safe_execute_core_init()` on Core 1 if missing.
3. Implement **refcounted pause** + discovery/ready/disconnect hooks matching Bluepad32 platform API.
4. Replace any `sleep_ms` in BT callbacks with `busy_wait_us`.
5. Tune **settle** (30 ms) and **resume delay** (100 ms); adjust per hardware if still flaky.
6. Run hardware matrix: **KB + mouse connected → pair gamepad → KB/mouse still work**; reboot reconnect; clear pairing keys.
7. Only then tune Core 0 poll rates or BTstack versions.

---

## References in this repo

| Document | Content |
|----------|---------|
| [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) | Apple ADB port status, checklist, Apple-specific fixes |
| [`bluetooth-pairing.md`](bluetooth-pairing.md) | User-facing pair order and troubleshooting |
| [`troubleshooting.md`](troubleshooting.md) § Bluetooth | Symptom → fix for multi-device pairing |
| [`FUTURE_WORK.md`](FUTURE_WORK.md) §1 | Pairing stability — done on `feature/BT-alignment` |
| [`gamepad-support.md`](gamepad-support.md) | BT gamepad Phase B + pairing hooks |

---

## Apple ADB extensions (beyond this handoff)

The Atari/Amiga recipe is necessary but not sufficient on the Apple ADB adapter. Also shipped on `feature/BT-alignment`:

| Extension | Why |
|-----------|-----|
| Always-merge BT keyboard + gamepad before `KeyboardPrs.Parse()` | Xbox-before-keyboard caused keyboard queue flood |
| `bluepad32_bt_defer_adb_reset()` + 2.5 s post-ready settle | Mac global ADB reset during BT pairing left mouse dead |
| `core1_force_release_bt_pause()` on disconnect / key wipe | Xbox sleep/wake reconnect with stuck pause depth |

Details: [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) § Apple ADB extensions.

---

## One-paragraph summary for paste into another LLM session

> On Pico 2 W + Bluepad32 + dual-core (Core 1 emulates from XIP flash), random BLE gamepad pairing hangs were a **multicore flash race**: BTstack writes pairing TLV via `flash_safe_execute` while Core 1 still executed from flash. Fix: `flash_safe_execute_core_init()` on Core 1; **refcounted** Core 1 pause on gamepad discovery; resume in `device_ready` after **100 ms busy-wait**; **30 ms settle** after pause; Core 1 pause loop uses **`__wfe()`**; BT callbacks use **`busy_wait_us` only** (never `sleep_ms`/`__wfe` on Core 0); no double-pause on connect; NVSettings sector must not overlap BTstack flash; keep BTstack at SDK pin; don’t call HID handlers twice per tick or drop to 2 ms HID without retesting pair. Debug `printf` masked the bug — use explicit delays and phase/pc diagnostics instead.
