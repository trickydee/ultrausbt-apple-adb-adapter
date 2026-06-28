# ADB shared GPIO rollback (device vs host)

**Branch:** `feature/fix-device-host-modes`  
**Date:** 2026-06-24  
**Related:** [`adb-host-mode.md`](adb-host-mode.md), release **2.0.0** (`3508160`)

## Problem

Firmware **2.0.0** changed ADB GPIO in **shared** files used by both modes:

| File | Change |
|------|--------|
| `src/firmware/lib/QuokkADB/include/adb_platform.h` | `data_hi()` tri-state (input) instead of drive-high; explicit `gpio_set_dir` in `data_lo()` / `adb_pin_*` |
| `src/firmware/lib/QuokkADB/src/quokkadb_gpio.cpp` | GP18 init released (input) instead of OUT high |

Host mode also needed **timing** fixes in `adb_host.cpp` only (765 µs attention, RX preamble). Testing showed host works with those timing changes; the shared tri-state GPIO likely **did not** fix host polling but **did** break device-mode collision detection (`adb_platform.cpp` IRQ checks `gpio_get(ADB_OUT_GPIO)` during “high” phases).

**Goal of this rollback:** restore pre-2.0 device GPIO behaviour in shared code; keep host open-collector GPIO and host timing in host-only code.

## What we kept (host-only, unchanged)

These stay in `src/firmware/lib/adb/src/adb_host.cpp`:

- **765 µs** attention low before `place_bit1()` (800 µs total per QMK/TMK)
- **RX preamble:** `wait_data_hi(500)` then `wait_data_lo(500)` before start-bit decode
- 65/35 µs bit cell timing in `place_bit0` / `place_bit1`

## What we reverted (shared → device mode)

### `adb_platform.h` — restored to 1.0.18 (`57ff5dd`)

```cpp
inline void AdbInterfacePlatform::data_lo()
{
    ADB_OUT_LOW();
}

inline void AdbInterfacePlatform::data_hi()
{
    ADB_OUT_HIGH();
}

inline void AdbInterfacePlatform::adb_pin_out()
{
}

inline void AdbInterfacePlatform::adb_pin_in()
{
}
```

### `quokkadb_gpio.cpp` — restored init

```cpp
void adb_gpio_init(void) {
    gpio_init(ADB_OUT_GPIO);
    gpio_set_function(ADB_OUT_GPIO, GPIO_FUNC_SIO);
    gpio_set_dir(ADB_OUT_GPIO, GPIO_OUT);
    gpio_put(ADB_OUT_GPIO, true);

    gpio_init(ADB_IN_GPIO);
    gpio_set_dir(ADB_IN_GPIO, GPIO_IN);
}
```

## What we moved (host-only)

Open-collector GPIO from 2.0.0 shared code now lives in:

**`src/firmware/lib/adb/include/adb_host_gpio.h`**

Used only by `adb_host.cpp` for TX, global reset, and bus wiring test. Device mode (`adb.cpp`) never includes this header.

### Archived 2.0.0 shared tri-state code (for restore)

If host mode regresses, compare or re-apply this pattern — either to `adb_host_gpio.h` (preferred) or back into shared `adb_platform.h` (not recommended; breaks device collision detection again).

```cpp
// adb_platform.h (2.0.0 shared — DO NOT re-apply to shared without mode split)
inline void AdbInterfacePlatform::data_lo()
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_OUT);
    ADB_OUT_LOW();
}

inline void AdbInterfacePlatform::data_hi()
{
    // Release the open-collector bus (adbuino / host pattern); external R2 pulls DATA high.
    gpio_set_dir(ADB_OUT_GPIO, GPIO_IN);
    gpio_disable_pulls(ADB_OUT_GPIO);
}

inline void AdbInterfacePlatform::adb_pin_out()
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_OUT);
    gpio_put(ADB_OUT_GPIO, true);
}

inline void AdbInterfacePlatform::adb_pin_in()
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_IN);
    gpio_disable_pulls(ADB_OUT_GPIO);
}
```

```cpp
// quokkadb_gpio.cpp (2.0.0 shared init)
void adb_gpio_init(void) {
    gpio_init(ADB_OUT_GPIO);
    gpio_set_function(ADB_OUT_GPIO, GPIO_FUNC_SIO);
    gpio_set_dir(ADB_OUT_GPIO, GPIO_IN);
    gpio_disable_pulls(ADB_OUT_GPIO);

    gpio_init(ADB_IN_GPIO);
    gpio_set_dir(ADB_IN_GPIO, GPIO_IN);
    gpio_disable_pulls(ADB_IN_GPIO);
}
```

## If host mode breaks after this rollback

1. **First check timing** — confirm `adb_host.cpp` still has 765 µs attention and RX preamble (unchanged by this rollback).
2. **UART debug** — flash `-host-debug.uf2`, watch for `Tlt`, `start`, `sync`, `BIT` RX errors during Talk R0.
3. **GPIO A/B** — temporarily change `adb_host_gpio::data_hi()` to drive-high (`ADB_OUT_HIGH()` + OUT dir) while keeping shared platform on device settings. If host works, tri-state on TX was the issue; if not, timing or wiring.
4. **Re-apply tri-state** — ensure `adb_host_gpio.h` matches the archived block above; do **not** put tri-state back into `adb_platform.h` without a runtime mode split.
5. **Git reference** — full 2.0.0 shared diff: `git show 3508160 -- src/firmware/lib/QuokkADB/include/adb_platform.h src/firmware/lib/QuokkADB/src/quokkadb_gpio.cpp`

## Test plan

| Mode | Build | Check |
|------|-------|-------|
| ADB → Mac (device) | `pico2_w-host.uf2` | USB mouse, **BT mouse** (no jumps), BT keyboard |
| ADB → USB (host) | same UF2, menu toggle | Keyboard + mouse + trackball poll; hot-plug rescan |
| Collision (optional debug) | `-host-debug.uf2` | No spurious `MOUSE: Collision on sending register 0` in device mode |

## Collision detection note

Device mode IRQ (`adb_platform.cpp`) treats a collision when:

- `collision_detection` is true,
- `gpio_get(ADB_OUT_GPIO)` is true (adapter driving high),
- `gpio_get(ADB_IN_GPIO)` falls (another device pulled DATA low).

With tri-state `data_hi()`, `gpio_out_high` is false while the bus is high via R2 → false collision signals or missed real ones. Drive-high restores the original QuokkADB behaviour.
