# Align IIgs ADB support with Apple IIgs Hardware Reference

This document records **gaps** between the current firmware (see `feature/IIGS-Fixes` timing work in `src/firmware/lib/adb/`) and **`docs/adb-iigs-hardware-reference.md`** (Apple IIgs Hardware Reference, Chapter 6, Table 6-8). It proposes **concrete code changes** and an **implementation plan**—no behavior is changed by this file alone.

**Primary spec:** Table 6-8 (*ADB timing specifications*) and duty-cycle definitions in Chapter 6.

**Related:** `docs/adb-iigs-support.md`, `docs/iigs-debugging.md`.

---

## 1. Current alignment (baseline)

These areas already match or sit inside the IIgs table:

| Item | Spec (IIgs HW Ref) | Current behavior |
|------|---------------------|------------------|
| Bit-cell total (receive) | 70–130 µs | Reject if `lo + hi` outside 70–130 in `ReceiveCommand` and `Receive16bitRegister` |
| Tlt (receive) | 140–260 µs | `Receive16bitRegister`: Tlt check 140–260 µs |
| Tlt (send) | 140–260 µs | `Send16bitRegister`: 140 + rand()%101 → 140–240 µs |
| SRQ duration (device) | 140–260 µs (table); ≥140 µs extension (prose) | `adb_delay_us(250)` after command when `srq` |
| Transmit bit cells | 70–130 µs cell; 0/1 lows 60–70% / 30–40% of cell | `place_bit0` / `place_bit1`: 65+35=100 µs, 35+65=100 µs |

---

## 2. Gaps and proposed changes

### 2.1 Attention window (Table 6-8: 560–1040 µs)

**Spec:** Attention low time **560–1040 µs**.

**Current:** `ReceiveCommand` accepts **ADB_ATTENTION_LO_MIN_US–1040 µs**  
(default `ADB_ATTENTION_LO_MIN_US=500`, reject if `lo < ADB_ATTENTION_LO_MIN_US` or `lo > 1040`).

**Issue:**

- **Low:** Default floor still allows **60 µs** shorter than the documented minimum (**500 vs 560**) as a deliberate measurement slack.
- **High:** **No current gap** on high end (accepts up to **1040 µs**).

**Proposed change:**

- Keep `1040` high bound.
- Evaluate whether default low floor should move from `500` toward `560` now that cross-host testing (IIgs + Quadra) looks stable; keep `ADB_ATTENTION_LO_MIN_US` configurable for A/B.
- Re-run validation on **real IIgs** and **68k Macs** after change; if 560 µs is too tight for one host, record measured min in `docs/iigs-debugging.md` and justify a deliberate floor.

**Files:** `src/firmware/lib/adb/src/adb.cpp` (`ReceiveCommand` attention loop).

---

### 2.2 Global reset threshold (Table 6-8: 2.8–5.2 ms; prose: ≥2.8 ms)

**Spec:** Global reset when bus held low **≥ 2.8 ms** (IIgs chapter prose).

**Current:** Global reset when **`lo >= 2800`** µs.

**Status:** Implemented and aligned with IIgs minimum.

**Follow-up:** Keep single-path handling so **attention** and **reset** remain mutually exclusive.

**Files:** `src/firmware/lib/adb/src/adb.cpp` (`ReceiveCommand`, same loop as attention).

---

### 2.3 Sync validation (Table 6-8: 60–70% of bit-cell time)

**Spec:** Sync high duration expressed as **60–70% of bit-cell time** (not only a fixed µs value).

**Current:** After `wait_data_lo(150)`, sync high time is validated as **40–95 µs** absolute.

**Issue:** Fixed µs is **not** the same test as **60–70% of cell**. For a **70 µs** cell, 60–70% → **42–49 µs**; for **130 µs** → **78–91 µs**. The fixed window overlaps but does not track the spec.

**Proposed change (choose one strategy):**

1. **Conservative:** Widen sync bounds to cover **42–91 µs** (envelope of 60–70% over 70–130 µs cells)—still approximate but bounded by Table 6-8.
2. **Spec-faithful:** After sync, measure or infer **bit-cell** from the **first command bit** (`lo + hi`), then re-validate sync **retrospectively** only if you can correlate edges (harder; may be unnecessary if (1) works on hardware).
3. **Document-first:** If (1) passes IIgs + Mac testing, keep fixed calibrated bounds but **comment** that they implement an envelope of **60–70% × [70,130] µs**.

**Files:** `src/firmware/lib/adb/src/adb.cpp` (`ReceiveCommand` sync section).

---

### 2.4 Bit decoding (Chapter 6: duty cycle)

**Spec:**

- Low **&lt; 35%** of bit-cell → **1**
- Low **&gt; 65%** of bit-cell → **0**

**Current:** After enforcing **70 ≤ lo+hi ≤ 130**, decode uses a **50% midpoint**  
(`lo * 100 < 50 * cell` => bit `1`, else bit `0`).

**Issue:** **40 µs** is a **fixed** threshold; the book uses **fraction of the same bit’s cell** (`lo / (lo+hi)`). At **130 µs** cell, 35% ≈ **45.5 µs**; a fixed **40** biases toward **0** for marginal cells.

**Current implementation status:**

- Added build-time option `ADB_STRICT_DUTY_CYCLE_DECODE`:
  - `ON`: spec-faithful decode: **1** if `<35%`, **0** if `>65%`, reject middle band.
  - `OFF` (default): midpoint/legacy decode at 50%.
- Implemented with integer math in both paths below.

**Files:**

- `src/firmware/lib/adb/src/adb.cpp` — command byte bits in `ReceiveCommand`
- `src/firmware/lib/adb/include/adb.h` — 16-bit register data bits in `Receive16bitRegister`

---

### 2.5 `Receive16bitRegister` start-bit sub-bounds

**Spec:** Table 6-8 does not list separate numeric sub-windows for the **start bit** edges; decoding is governed by **bit-cell** and **%** rules.

**Current:** Fixed checks: start low **18–55 µs**, following high **40–90 µs**.

**Proposed change:**

- After **2.4** is implemented, evaluate whether these fixed windows are **redundant** with cell + % checks or **over-constrain** valid IIgs waveforms.
- Prefer **one** validation story: **cell total** + **duty** + **Tlt**, and remove or relax redundant fixed windows if tests pass.

**Files:** `src/firmware/lib/adb/include/adb.h` (`Receive16bitRegister`).

---

### 2.6 Stop bit receive (`Receive16bitRegister`)

**Spec:** Our `adb-iigs-hardware-reference.md` excerpt does **not** duplicate *Guide*’s **70 µs** stop-bit row; IIgs Chapter 6 may still agree in spirit.

**Current:** `wait_data_hi(130)` then reject if stop low **> 85 µs**.

**Proposed change:**

- Cross-check **IIgs PDF** (Chapter 6 figures/tables) for an explicit stop-bit row. If present, match it; if not, keep **70 µs** as *Guide*-compatible and **note** in firmware comment that stop timing follows *Guide* unless IIgs states otherwise.

**Files:** `src/firmware/lib/adb/include/adb.h`.

---

### 2.7 SRQ comment drift (300 µs vs 140–260 µs)

**Current:** Comments reference **300 µs** SRQ in places (*Guide* Table 8-14 style).

**Proposed change:**

- Update comments to **IIgs Table 6-8 (140–260 µs)** and **≥140 µs** extension prose; mention *Guide* **300 µs ±30%** only as secondary context for non-IIgs hosts.

**Files:** `src/firmware/lib/adb/src/adb.cpp` (post–`ReceiveCommand` stop handling).

---

### 2.8 IIgs policy: mouse must not assert SRQ

**Spec:** IIgs Hardware Reference — **ADB mouse** must **not** issue Service Requests on the IIgs.

**Current / resolved:** Implemented an IIgs-appropriate policy switch `ADB_IIGS_MOUSE_SUPPRESS_SRQ`.

- When `ADB_IIGS_MOUSE_SUPPRESS_SRQ=ON`, the adapter suppresses the mouse SRQ extension so `ReceiveCommand()` is driven by keyboard SRQ only.
- **Default:** `ON` in `src/firmware/CMakeLists.txt` for all configures (including `build_all.sh`, `make`, and CI). Use `-DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF` only when you need legacy mouse SRQ behavior.

**Validation:** On your IIgs (Taifun Boot) the BASIC loop slowdown on mouse move is resolved. It also remains good on an ADB Mac Quadra.

---

## 3. Implementation plan

### Phase A — Low risk, high clarity

1. Re-evaluate **attention** low default (`ADB_ATTENTION_LO_MIN_US=500`) against **560 µs** spec floor using IIgs + Mac A/B data.
2. Keep **global reset** at **>=2800 µs** (already aligned).
3. Refresh **comments** for SRQ (**140–260 / >=140**) vs legacy 300 µs.

**Exit criteria:** Build clean; smoke-test on **IIgs** and at least one **68000-era Mac** with ADB.

---

### Phase B — Decode correctness

4. Implement **percentage-based** bit decode (**35% / 65%**) for **command** and **16-bit register** receives. ✅  
5. Re-test fast typing / missed keys (`docs/iigs-debugging.md` scenarios) with `ADB_STRICT_DUTY_CYCLE_DECODE` ON vs OFF.

**Exit criteria:** No regression on USB kbd/mouse → ADB on IIgs; UART debug shows no spike in `ADB RX fail: BIT`.

---

### Phase C — Sync and 16-bit path cleanup

6. Replace or document **sync** window: envelope **42–91 µs** or **%**-based approach (**2.3**).
7. Revisit **`Receive16bitRegister`** start-bit fixed limits vs unified cell/% rules (**2.5**).
8. Confirm **stop-bit** limits against IIgs PDF (**2.6**).

**Exit criteria:** Talk Register 0/3 and keyboard path stable on IIgs.

---

### Phase D — Optional product policy

9. Decide on **mouse SRQ** gating for “IIgs mode” (**2.8**).

---

## 4. Testing checklist

- **Apple IIgs:** boot, keyboard, mouse, extended typing, SRQ-heavy use.
- **ADB Mac** (e.g. SE/30 or LC): regression on **800 µs**-style attention (should still pass **560–1040**).
- **UART** (`ADB_DEBUG` builds): compare `ADB RX fail:` rates before/after each phase.

---

## 5. Documentation updates after code lands

- `docs/adb-iigs-support.md` — reflect final thresholds and any intentional slack.
- `docs/adb-iigs-hardware-reference.md` — add a short “Firmware mapping” subsection if helpful (which `#define` or checks correspond to each table row).
