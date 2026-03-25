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

**Current:** `ReceiveCommand` accepts **500–950 µs** (reject if `lo < 500` or `lo > 950`).

**Issue:**

- **Low:** Allows **40 µs** shorter than the documented minimum (**500 vs 560**).
- **High:** Rejects valid attention up to **1040 µs** (anything **951–1040 µs** fails).

**Proposed change:**

- Replace bounds with **560–1040 µs**, or **559–1041 µs** if reserving 1 µs slack for sampling jitter (document the slack in code comments).
- Re-run validation on **real IIgs** and **68k Macs** after change; if 560 µs is too tight for one host, record measured min in `docs/iigs-debugging.md` and justify a deliberate floor.

**Files:** `src/firmware/lib/adb/src/adb.cpp` (`ReceiveCommand` attention loop).

---

### 2.2 Global reset threshold (Table 6-8: 2.8–5.2 ms; prose: ≥2.8 ms)

**Spec:** Global reset when bus held low **≥ 2.8 ms** (IIgs chapter prose).

**Current:** Global reset when **`lo > 2950`** µs (~2.95 ms).

**Issue:** A reset pulse of **~2.8–2.95 ms** may be classified as a failed attention instead of **`-100`** (global reset).

**Proposed change:**

- Treat global reset when **`lo >= 2800`** µs (or **> 2790** if using strict “above 2.8 ms”), consistent with IIgs minimum.
- Keep a single code path so **attention** (short low) and **reset** (long low) stay mutually exclusive; add a short comment citing **2.8 ms** vs legacy **~3 ms** behavior.

**Files:** `src/firmware/lib/adb/src/adb.cpp` (`ReceiveCommand`, same loop as attention).

---

### 2.3 Sync validation (Table 6-8: 60–70% of bit-cell time)

**Spec:** Sync high duration expressed as **60–70% of bit-cell time** (not only a fixed µs value).

**Current:** After `wait_data_lo(150)`, sync high time validated as **25–105 µs** absolute.

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

**Current:** After enforcing **70 ≤ lo+hi ≤ 130**, decode with **`lo < 40`** → bit 1.

**Issue:** **40 µs** is a **fixed** threshold; the book uses **fraction of the same bit’s cell** (`lo / (lo+hi)`). At **130 µs** cell, 35% ≈ **45.5 µs**; a fixed **40** biases toward **0** for marginal cells.

**Proposed change:**

- Replace with percentage-based decode, e.g.  
  - `cell = lo + hi` (already constrained)  
  - If `lo * 100 < 35 * cell` (or fixed-point equivalent) → **1**  
  - Else if `lo * 100 > 65 * cell` → **0**  
  - Else → **error** (illegal duty cycle for valid cell)
- Use integer math only (no float on hot path if that matters for latency).

**Files:**

- `src/firmware/lib/adb/src/adb.cpp` — command byte bits in `ReceiveCommand`
- `src/firmware/lib/adb/include/adb.h` — 16-bit register data bits in `Receive16bitRegister`

---

### 2.5 `Receive16bitRegister` start-bit sub-bounds

**Spec:** Table 6-8 does not list separate numeric sub-windows for the **start bit** edges; decoding is governed by **bit-cell** and **%** rules.

**Current:** Fixed checks: start low **25–45 µs**, following high **55–75 µs** (legacy QuokkADB-style).

**Proposed change:**

- After **2.4** is implemented, evaluate whether these fixed windows are **redundant** with cell + % checks or **over-constrain** valid IIgs waveforms.
- Prefer **one** validation story: **cell total** + **duty** + **Tlt**, and remove or relax redundant fixed windows if tests pass.

**Files:** `src/firmware/lib/adb/include/adb.h` (`Receive16bitRegister`).

---

### 2.6 Stop bit receive (`Receive16bitRegister`)

**Spec:** Our `adb-iigs-hardware-reference.md` excerpt does **not** duplicate *Guide*’s **70 µs** stop-bit row; IIgs Chapter 6 may still agree in spirit.

**Current:** `wait_data_hi(130)` then reject if stop low **> 70 µs**.

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

**Current:** Firmware tracks `mousesrq` / `kbdsrq` with no host identity.

**Proposed change:**

- **Documentation:** Describe behavior in `docs/adb-iigs-support.md` (HIDHopper is often kbd+mouse; host may be IIgs or Mac).
- **Optional product switch:** Build-time or runtime flag **“Assume IIgs host”** to suppress SRQ on the mouse path only—**only if** you confirm with testing that IIgs hosts misbehave otherwise.

**Files:** Design doc + possibly `adb.cpp` / USB bridge—**lower priority** than timing items **2.1–2.4**.

---

## 3. Implementation plan

### Phase A — Low risk, high clarity

1. Update **attention** bounds to **560–1040 µs** (with optional ±1 µs slack documented).
2. Lower **global reset** threshold to **~2800 µs** (align with **2.8 ms**).
3. Refresh **comments** for SRQ (**140–260 / ≥140**) vs legacy 300 µs.

**Exit criteria:** Build clean; smoke-test on **IIgs** and at least one **68000-era Mac** with ADB.

---

### Phase B — Decode correctness

4. Implement **percentage-based** bit decode (**35% / 65%**) for **command** and **16-bit register** receives.
5. Re-test fast typing / missed keys (`docs/iigs-debugging.md` scenarios).

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
