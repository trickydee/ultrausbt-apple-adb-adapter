# ADB receive timing: Apple IIgs support (`feature/IIGS-Fixes`)

This document summarizes how ADB **host→device receive** behavior on branch **`feature/IIGS-Fixes`** differs from **`feature/display`** (and the same code paths before the IIgs-oriented commits). The goal is reliable command and register reads when the host is an Apple IIgs, whose attention and bit timing can differ from “typical” 68000-era Macs.

Implementation lives in:

- `src/firmware/lib/adb/src/adb.cpp` — `ReceiveCommand`
- `src/firmware/lib/adb/include/adb.h` — `Receive16bitRegister` (inline)

For follow-on tuning ideas (fast typing, SRQ, main loop), see `docs/iigs-debugging.md`.

Published **Apple IIgs Hardware Reference** timing (Table 6-8 and Chapter 6) is summarized in `docs/adb-iigs-hardware-reference.md`.

---

## Field validation snapshot

Recent bench testing of the same IIgs-tuned firmware on both:
- Apple IIgs (Taifun Boot GUI and general keyboard/mouse use), and
- ADB Mac Quadra,

showed improved behavior versus earlier builds, including smoother overall host interaction. This suggests the current receive-timing robustness changes are broadly compatible with classic ADB hosts and are not IIgs-only regressions.

It also includes the IIgs mouse policy `ADB_IIGS_MOUSE_SUPPRESS_SRQ` (default **ON** in CMake), which prevents mouse SRQ activity from slowing the BASIC loop when the mouse is moved.

---

## 1. Attention pulse (`ReceiveCommand`)

The firmware waits for the **attention** interval (data line held low by the host before the sync period).

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Accepted low time | ~780–820 µs (`lo > 820 \|\| lo < 780`) | **`ADB_ATTENTION_LO_MIN_US`–1040 µs** (default floor `500`) |

**Why:** The IIgs often presents attention closer to **~500–650 µs** rather than a tight band around 800 µs. Widening the window avoids rejecting valid IIgs frames while still requiring a plausible attention duration. The low floor is intentionally configurable for A/B testing (`ADB_ATTENTION_LO_MIN_US`).

Unchanged behavior intent: very long lows still indicate **global reset** (`return -100`); current threshold is **>= 2800 µs**.

---

## 2. Sync / start bit (`ReceiveCommand`)

After attention, the host drives **sync** (high) then the **start** bit.

| | `feature/display` | `feature/IIGS-Fixes` |
|---|-------------------|----------------------|
| Wait for high→low | `wait_data_lo(100)` | **`wait_data_lo(150)`** (more time for long sync) |
| Valid high duration | Buggy check: `if (!hi && hi > 70 && hi < 40)` (never true) | **`40–95 µs`** (`!hi \|\| hi > 95 \|\| hi < 40` rejects) |

**Why:** IIgs timing for this phase can be shorter or need a longer wait before the edge is seen; the current fixed window is an envelope tuned around IIgs-compatible sync timing.

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

This file (`docs/adb-iigs-support.md`) is a **high-level summary** only; exact thresholds should always be taken from the source on the branch you are building. Current build-time options that affect timing/behavior include `ADB_ATTENTION_LO_MIN_US` and `ADB_MOUSE_ACCUMULATE_DELTAS`.
