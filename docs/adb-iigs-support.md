# ADB receive timing: Apple IIgs support (`feature/IIGS-Fixes`)

This document summarizes how ADB **host→device receive** behavior on branch **`feature/IIGS-Fixes`** differs from **`feature/display`** (and the same code paths before the IIgs-oriented commits). The goal is reliable command and register reads when the host is an Apple IIgs, whose attention and bit timing can differ from “typical” 68000-era Macs.

Implementation lives in:

- `src/firmware/lib/adb/src/adb.cpp` — `ReceiveCommand`
- `src/firmware/lib/adb/include/adb.h` — `Receive16bitRegister` (inline)

For follow-on tuning ideas (fast typing, SRQ, main loop), see `docs/iigs-debugging.md`.

---

## 1. Attention pulse (`ReceiveCommand`)

The firmware waits for the **attention** interval (data line held low by the host before the sync period).

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Accepted low time | ~780–820 µs (`lo > 820 \|\| lo < 780`) | **500–950 µs** |

**Why:** The IIgs often presents attention closer to **~500–650 µs** rather than a tight band around 800 µs. Widening the window avoids rejecting valid IIgs frames while still requiring a plausible attention duration.

Unchanged: very long lows (e.g. **> 2950 µs**) still indicate **global reset** (`return -100`).

---

## 2. Sync / start bit (`ReceiveCommand`)

After attention, the host drives **sync** (high) then the **start** bit.

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Wait for high→low | `wait_data_lo(100)` | **`wait_data_lo(150)`** (more time for long sync) |
| Valid high duration | Buggy check: `if (!hi && hi > 70 && hi < 40)` (never true) | **`25–105 µs`** (`!hi \|\| hi > 105 \|\| hi < 25` rejects) |

**Why:** IIgs timing for this phase can be shorter or need a longer wait before the edge is seen; the new bounds match an IIgs-oriented range noted in commit history (previously documented as widening from ~30–95 µs to **25–105 µs**).

---

## 3. Command bit cells (`ReceiveCommand`)

For each of the 8 command bits, low and high times are measured and summed.

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Bit cell total | Reject if `120 < lo + hi` (only upper bound in practice) | **70–130 µs** (`lo + hi < 70 \|\| 130 < lo + hi` rejects) |

**Why:** Explicit **min and max** per bit cell aligns checks with the **Apple IIgs Hardware Reference**–style **100 µs** nominal cell with margin, and avoids accepting or rejecting edge cases inconsistently.

---

## 4. 16-bit register receive (`Receive16bitRegister`)

Used when the host **Talk**s a 16-bit register (e.g. keyboard/mouse data).

### Stop-to-start (Tlt)

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Tlt (high after stop, before start) | 130–270 µs | **140–260 µs** (comment: spec “officially 140–260 µs”) |

### Data bit cells

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Per-bit total | Reject if `120 < lo + hi` | **70–130 µs** (same explicit bounds as command receive) |

**Why:** The first IIgs commit widened some receive paths; a follow-up commit tightened **Tlt** and **command** bit cells back toward spec while keeping **IIgs-friendly attention and sync** handling in `ReceiveCommand`.

---

## 5. Debug logging (optional)

When `global_debug` is true, failed receives log short UART lines instead of older verbose messages:

- `ADB RX fail: ATTENTION lo=<µs>`
- `ADB RX fail: SYNC hi=<µs>`
- `ADB RX fail: BIT b=<bit> lo=<µs> hi=<µs>`

**Build with ADB UART debug:** CMake option **`ADB_DEBUG=ON`** defines `ADB_DEBUG` and forces **`global_debug = true`** in `src/firmware/lib/QuokkADB/src/quokkadb.cpp`. **`build_all.sh`** on this branch also produces per-board **`build-<board>-debug`** and copies **`HIDHopper-firmware-debug.uf2`**.

---

## 6. Related commits (on `feature/IIGS-Fixes`)

1. **IIGS ADB: wider timing, targeted RX failure logging** — attention window, sync/start bounds, bit-cell and logging changes; `ADB_DEBUG` / `build_all.sh` debug builds.
2. **IIGS: spec-aligned receive timing + debugging doc** — Tlt and register/command bit cells adjusted; adds `docs/iigs-debugging.md`.

---

## 7. Files touched for ADB + debug tooling

| File | Role |
|------|------|
| `src/firmware/lib/adb/src/adb.cpp` | `ReceiveCommand` timing and RX-fail logging |
| `src/firmware/lib/adb/include/adb.h` | `Receive16bitRegister` Tlt and bit-cell checks |
| `src/firmware/CMakeLists.txt` | `ADB_DEBUG` option |
| `src/firmware/lib/QuokkADB/src/quokkadb.cpp` | `global_debug` default when `ADB_DEBUG` |
| `build_all.sh` | Release + debug tree builds |
| `docs/iigs-debugging.md` | Further IIgs debugging parameters |

This file (`docs/adb-iigs-support.md`) is a **high-level summary** only; exact thresholds should always be taken from the source on the branch you are building.
