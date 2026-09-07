# Bluetooth pairing — port Atari v22.1.0 (Amiga reference implementation)

**Apple ADB status:** **Port complete** on branch **`feature/BT-alignment`** — see [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) for checklist and Apple-specific extensions (keyboard/gamepad merge, defer Mac ADB reset).

**Audience:** LLM or developer porting **ultrausbt-apple-adb-adapter** to the same BLE gamepad pairing fix already shipped on Atari ST (v22.1.0) and **ultrausbt-amiga** (`feature/BT-Pairing-align`).

**Start here:** [`BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) (family-wide root cause).  
**This repo status:** [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) (port complete + Apple extras).  
**Canonical source:** `ultrausbt-atari-st-rpikbd` — tag/branch with **v22.1.0** pairing fix.  
**Sibling port (completed):** `ultrausbt-amiga` — branch **`feature/BT-Pairing-align`** (firmware **v2.2.11**).

---

## Why Amiga is the best second reference

Apple ADB and Amiga share the same **pre-fix** pattern:

- Single `bool` pause flag
- `sleep_ms()` in Bluepad32 callbacks
- Double-pause on Xbox/Stadia `device_connected`
- `busy_wait_us` spin in Core 1 pause loop (not `__wfe()`)
- Short (10–50 ms) resume delays instead of 100 ms `busy_wait_us`

Amiga’s port is a **direct C translation** of Atari’s `main.cpp` + `bluepad32_platform.c`, adapted for **quadrature mouse on Core 1** instead of HD6301. Apple ADB’s equivalent is **`tuh_task()` on Core 1** — the pause mechanism is the same multicore flash race.

---

## Architecture mapping (three products)

| Piece | Atari ST | Amiga (ported) | Apple ADB (ported) |
|-------|----------|----------------|---------------------|
| **Core 0** | TinyUSB, Bluepad32, OLED | Same | ADB, Bluepad32, OLED |
| **Core 1 from XIP** | HD6301 emulator | Quadrature mouse GPIO | `tuh_task()` + keyboard LEDs |
| **What to pause** | Emulator loop | Mouse quadrature loop | USB host loop (`tuh_task`) |
| **Pause module** | `main.cpp` | `quad_mouse.c` | **`bt_host_coop.c`** |
| **Platform hooks** | `bluepad32_platform.c` | `bluepad32_platform.c` | `src/firmware/src/bluepad32_platform.c` |
| **User flash** | `NVSettings.cpp` | `mouse_config.c` (moved below TLV) | `flashsettings.cpp` (**already OK**) |
| **System clock** | 225 MHz (BT builds) | 200 MHz | 125 MHz — retest if hangs persist |

---

## Amiga files changed (copy patterns from here)

| File | What was done |
|------|----------------|
| `src/config.h` | `BT_GAMEPAD_DISCOVERY_SETTLE_MS` (30), `BT_GAMEPAD_CORE1_RESUME_DELAY_MS` (100), `BT_CORE1_PAUSE_WATCHDOG_MS` (45000) |
| `src/platform/amiga/quad_mouse.c` | Refcount `g_bt_pause_depth`; separate `g_cd32_pause`; `__wfe()` in pause branch; watchdog |
| `src/platform/amiga/quad_mouse.h` | `core1_pause_for_bt_enumeration()`, `core1_resume_after_bt_enumeration()`, `core1_wait_for_pause_active()`, `core1_get_bt_pause_depth()`, `core1_force_release_bt_pause()`, `core1_bt_pause_watchdog_tick()` |
| `src/bluepad32_platform.c` | `bt_callback_busy_wait_ms()`; discovery settle; **no** connect pause; unified resume in `device_ready`; disconnect resume if depth > 0 |
| `src/main.c` | `core1_bt_pause_watchdog_tick()` + `port_config_flush_pending()` each loop |
| `src/platform/amiga/mouse_config.c` | Flash sector below BTstack TLV; defer save while `core1_get_bt_pause_depth() > 0` |

**Apple ADB does not need CD32 split** — only refcount BT pause in `bt_host_coop`. No second pause reason unless you add a future feature that pauses Core 1 independently.

---

## Step-by-step port order (Apple ADB)

### 1. Extend `bt_host_coop.c` / `bt_host_coop.h` (≈ Atari `main.cpp` pause API)

Replace single bool with:

```c
void core1_pause_for_bt_enumeration(void);   // ++depth, set paused
void core1_resume_after_bt_enumeration(void); // --depth if > 0
void core1_wait_for_pause_active(uint32_t timeout_ms);
uint32_t core1_get_bt_pause_depth(void);
void core1_force_release_bt_pause(void);
bool core1_bt_pause_watchdog_tick(void);
```

Keep `bt_host_coop_usb_host_is_paused()` as `return g_bt_pause_depth > 0` (or alias to combined paused flag).

Add diagnostics (optional but useful): `g_core1_pause_spins`, `CORE1_PHASE_PAUSED` — Atari uses these in `core1_wait_for_pause_active()`.

### 2. `quokkadb.cpp` — Core 1 pause loop

**Today** (`core1_main`):

```cpp
if (bt_host_coop_usb_host_is_paused()) {
  busy_wait_us(5000);
  continue;
}
```

**Target** (match Amiga `quad_mouse.c` / Atari `core1_entry`):

```cpp
if (bt_host_coop_usb_host_is_paused()) {
  g_core1_pause_spins++;
  __wfe();  // yield for flash_safe_execute multicore lockout
  continue;
}
```

`flash_safe_execute_core_init()` at Core 1 entry — **already present**.

### 3. `bluepad32_platform.c` — platform callbacks

Port **line-for-line logic** from Amiga `src/bluepad32_platform.c` on `feature/BT-Pairing-align`:

| Callback | Action |
|----------|--------|
| `on_init_complete` | `bt_callback_busy_wait_ms(2000)` — **not** `sleep_ms(2000)` |
| `on_device_discovered` | If COD `0x0508` or name contains Stadia/Xbox: `core1_pause_for_bt_enumeration()` → `core1_wait_for_pause_active(20)` → `bt_callback_busy_wait_ms(30)` |
| `on_device_connected` | **Empty** — do not pause again |
| `on_device_disconnected` | If `core1_get_bt_pause_depth() > 0`: `core1_resume_after_bt_enumeration()` |
| `on_device_ready` | Register device; then if `depth > 0`: `bt_callback_busy_wait_ms(100)` → `core1_resume_after_bt_enumeration()` — **all device types**, not gamepad-only |
| `bluepad32_delete_pairing_keys` | `core1_force_release_bt_pause()` before `uni_bt_del_keys_unsafe()` |

Add helper (same as Amiga):

```c
static void bt_callback_busy_wait_ms(uint32_t ms) {
    busy_wait_us(ms * 1000u);
}
```

**Remove:** Xbox/Stadia-only branches with `sleep_ms(50)` / `sleep_ms(10)` and second pause on connect.

### 4. Config constants

Add to a header (e.g. `display_config.h` or new `bt_pairing_config.h`):

```c
#define BT_GAMEPAD_DISCOVERY_SETTLE_MS      30
#define BT_GAMEPAD_CORE1_RESUME_DELAY_MS    100
#define BT_CORE1_PAUSE_WATCHDOG_MS          45000
```

### 5. Main loop (`adb_mode.cpp` or equivalent Core 0 loop)

Each iteration when Bluepad32 enabled:

```c
core1_bt_pause_watchdog_tick();
```

If you ever save user settings during pairing, defer flash writes while `core1_get_bt_pause_depth() > 0` (Amiga pattern in `mouse_config.c`). **`flashsettings.cpp` sector layout is already below TLV** — no overlap fix needed.

### 6. Do **not** repeat failed experiments

Documented in [`changes.md`](changes.md) and Amiga [`doc/cleanup.md`](../../../ultrausbt-amiga/doc/cleanup.md) (if available):

- Shorter delays (10 ms resume) without full recipe
- `__not_in_flash_func` on pause path only
- Disabling OLED during pair without refcount/`__wfe()`
- 2 ms HID polling / double handler calls per tick

---

## Discovery filter (use Atari/Amiga, not old Amiga pre-fix)

Pause only for:

- COD **`0x0508`**, or
- Name contains **`Stadia`**, **`Xbox`**, **`XBOX`**

Do **not** pause for generic `"gamepad"` name strings — reduces false pauses.

---

## Hardware test matrix (required before merge)

| # | Setup | Action | Pass |
|---|-------|--------|------|
| 1 | BT keyboard only | Pair MX Keys (or similar) | KB works; Core 1 USB not stuck |
| 2 | KB + BT mouse | Pair mouse | Both work |
| 3 | KB + mouse | Pair **Stadia** BLE | Gamepad works; USB/BT inputs still work on ADB |
| 4 | Same | Reboot | Stadia reconnects |
| 5 | Same | Pair **Xbox** BLE | Same as Stadia |
| 6 | Stale bond | Clear keys (UI flow) + re-pair Stadia | No re-encrypt loop |
| 7 | After failed pair | Power off gamepad | ADB mouse/keyboard recover (watchdog ≤ 45 s) |

Test on **Pico W** and **Pico 2 W** if both are supported targets.

---

## Symptom → fix quick reference

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Hang during Stadia GATT / pairing | Core 1 still in XIP during TLV flash write | Pause + `__wfe()` + settle |
| Mouse/USB dead, buttons OK | Core 1 paused, not resumed | Refcount; resume in `device_ready` **and** disconnect; watchdog |
| `heartbeat unchanged` after resume | Core 1 wedged in XIP, not pause spin | `__wfe()` not busy-spin; 100 ms busy_wait before resume |
| PS5 works, Stadia/Xbox fail | Longer BLE bond path | Same fixes — not a different stack |
| Debug `printf` “fixes” hang | Heisenbug — timing mask | Use explicit delays, not more logging |

---

## Reference file paths (for diff)

| Repo | Path |
|------|------|
| **Atari (canonical)** | `src/main.cpp` (~207–252), `src/bluepad32_platform.c` (~308–503), `include/config.h` |
| **Amiga (sibling port)** | `src/platform/amiga/quad_mouse.c`, `src/bluepad32_platform.c`, `src/config.h` on branch `feature/BT-Pairing-align` |
| **Apple ADB (this repo)** | `lib/QuokkADB/src/quokkadb.cpp`, `src/bt_host_coop.c`, `src/bluepad32_platform.c` |

---

## LLM starter prompt (paste into Apple ADB session)

> **Context:** ultrausbt-apple-adb-adapter (Pico W / Pico 2 W). Core 0 = ADB + Bluepad32 + OLED. Core 1 = TinyUSB `tuh_task()` from XIP (`quokkadb.cpp`). **Problem:** BLE gamepad pairing (Stadia, Xbox) hangs or leaves USB host frozen while KB/mouse on BT work. **Root cause:** multicore flash race — BTstack TLV writes via `flash_safe_execute` while Core 1 runs from flash. **Canonical fix:** Atari v22.1.0. **Completed sibling port:** ultrausbt-amiga branch `feature/BT-Pairing-align` (v2.2.11). **Read:** `docs/BT_PAIRING_HANDOFF.md`, `docs/BT_PAIRING_PORT_AMIGA_REFERENCE.md`, `docs/BT_PAIRING_APPLE_ADB.md`. **Port:** refcounted pause in `bt_host_coop.c`; `__wfe()` in Core 1 pause loop; `bt_callback_busy_wait_ms` only in `bluepad32_platform.c`; 30 ms discovery settle + 100 ms pre-resume; no double-pause on connect; watchdog + force resume on key wipe. **Flash:** `flashsettings.cpp` already below TLV — no sector move. **Test:** KB + mouse + Stadia + Xbox on hardware before merge.

---

## After porting

- [x] [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) checklist marked **Done** (plus Apple-specific extensions documented).
- [x] [`FUTURE_WORK.md`](FUTURE_WORK.md) §1 updated to **Done** on `feature/BT-alignment`.
- [x] Merge to `main`; bump `CMakeLists.txt` to **2.2.1**; update [`release-notes.md`](release-notes.md).
- [ ] Note clock speed (125 MHz vs Atari 225 MHz) if hangs persist — only after full recipe is green on release hardware.

**Apple ADB extensions beyond this Amiga port:** always-merge keyboard + gamepad in `bt_hid_bridge.cpp`; `bluepad32_bt_defer_adb_reset()` during Mac global ADB reset windows. See [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) § Apple ADB extensions.
