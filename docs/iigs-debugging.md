# IIGS ADB Keyboard Debugging

Parameters and ideas to try when the keyboard works when typing slowly but occasionally misses key down/up when typing fast. Use an oscilloscope to confirm whether the issue is our response timing or the host missing our reply.

---

## 1. Main-loop / ADB polling

**Idea:** When you type fast, the main loop may be busy (USB, Bluetooth, key handling, Serial) and sometimes miss the next host poll.

**Parameters to consider:**
- Poll ADB receive in a tighter loop or more often (e.g. poll ADB every time with minimal work in between).
- Reduce work per iteration (throttle or disable Serial when not debugging).
- Run ADB on the other core so it’s not competing with USB/BT.

---

## 2. Tlt (delay before we send our reply)

**Current:** 140–240 µs (140 + 0–100 µs random) before we drive our first bit.

**Idea:** IIGS might need a bit more time to be ready to sample; responding “too soon” could cause an occasional miss when traffic is higher.

**Parameters to consider:**
- Increase minimum delay (e.g. 160–200 µs minimum instead of 140 µs).
- Or use a fixed delay in the middle of the 140–260 µs window (e.g. ~200 µs) instead of random.

---

## 3. Key event / register 0 handling

**Idea:** When you type fast, we might overwrite `kbdreg0` with the next key before the IIGS has polled the previous key-up, so one key-up never gets sent.

**Parameters to consider:**
- Keep a small queue of “pending register 0” states (e.g. 2) and send key-down then key-up in order.
- Or don’t replace `kbdreg0` until we’ve “had a chance” to be polled (e.g. simple delay or flag that we only clear after the next poll attempt).

---

## 4. SRQ (service request) hold time

**Current:** We hold the line low 250 µs for SRQ. Spec is 300 µs ±30% (210–390 µs).

**Idea:** Under load, the host might sample SRQ slightly late; a longer hold might be more reliable.

**Parameter to consider:** Increase SRQ low time toward 280–300 µs (e.g. change the 250 µs delay to 280 or 300 µs).

---

## 5. Our transmit bit timing

**Current:** 35 µs low / 65 µs high for ‘1’, 65 µs low / 35 µs high for ‘0’ (100 µs bit cell).

**Idea:** IIGS might sample at a different phase; slightly longer low/high times could give more margin.

**Parameters to consider:**
- Slightly stretch the bit cell (e.g. 70 µs low / 60 µs high for ‘0’ and 40 µs / 70 µs for ‘1’) so edges are a bit later.
- Or add a small delay before we start sending our first bit after Tlt.

---

## 6. Receive timing strictness (if we see RX errors again)

**Current:** Tlt 140–260 µs and bit cell 70–130 µs. If the IIGS is borderline under load, we might reject valid frames.

**Parameters to consider:** Slightly relax again (e.g. Tlt 130–270 µs, or bit cell 65–135 µs) only if logs show SYNC/BIT or Receive16bitRegister failures when you type fast.

**Phase-B decode toggle:** `ADB_STRICT_DUTY_CYCLE_DECODE`
- `ON`: strict 35%/65% decode (reject ambiguous middle duty cycle).
- `OFF`: tolerant midpoint decode.
Use A/B testing on real hardware if strict mode increases `ADB RX fail: BIT`.

---

## 7. Debug / Serial

**Idea:** With `global_debug` on, `Serial.print` can block and delay the next ADB poll.

**Parameter to consider:** Use a non-debug build (or turn debug off) for normal typing tests so Serial isn’t in the path.

---

## 8. Bluetooth vs USB keyboard

**Idea:** If the source is a Bluetooth keyboard, Bluepad32 and radio traffic can add jitter and delay the main loop.

**Parameter to consider:** For comparison, try a USB keyboard and see if “fast typing” is more reliable; that would point to main-loop/polling or BT load.

---

## 9. Mouse delta accumulation mode (IIgs pointer smoothness)

**Observation:** On some IIgs GUIs, the pointer can look jerky or briefly redraw/glitch when USB/BLE mouse reports arrive faster than ADB polls.

**Build-time option:** `ADB_MOUSE_ACCUMULATE_DELTAS`
- `ON` (default): accumulate mouse `dx/dy` between ADB polls with saturation.
- `OFF`: legacy behavior that keeps only the latest `dx/dy` between polls.

**Why this helps:** Accumulation reduces dropped micro-movements when host poll cadence is lower than mouse report cadence.

**How to build:**
- Enable (default): `cmake -B build -S src/firmware -DPICO_BOARD=pico2_w`
- Disable: `cmake -B build -S src/firmware -DPICO_BOARD=pico2_w -DADB_MOUSE_ACCUMULATE_DELTAS=OFF`

For A/B testing with existing script-driven flows, keep all other options identical and toggle only this setting.

---

## 10. Mouse SRQ suppression (IIgs BASIC slowdown)

**Observation:** On the IIgs, allowing the mouse path to extend/trigger SRQ can cause the BASIC program loop to slow down massively as soon as mouse movement/buttons begin.

**Build-time option:** `ADB_IIGS_MOUSE_SUPPRESS_SRQ`
- `ON` (**default** in `CMakeLists.txt`): suppress mouse SRQ extension (keyboard SRQ only). Recommended for IIgs and validated on ADB Macs.
- **Hub mode + chained trackball:** mouse SRQ is not enabled on the wire (hub SRQ workaround reverted). After trackball idle, USB/BT mouse may need a keyboard event to wake the bus until §9 intelligent detection lands.
- `OFF`: legacy behavior (mouse can also extend SRQ). Pass `-DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF` if you need to compare or hit an edge case.

**Disable example:**  
`cmake -B build -S src/firmware -DPICO_BOARD=pico2_w -DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF`

**Validation:** Confirmed improved BASIC performance on an IIgs (Taifun Boot) and also still works well on a Mac Quadra.

---

## 11. Bluetooth mouse jumps (device mode, firmware 2.0.0)

**Symptom:** USB mouse OK; BT mouse **jumps** intermittently after upgrading to **2.0.0** host release.

**Cause / fix:** Shared tri-state ADB GPIO broke collision detection. Fixed in **2.1.0** (drive-high in `adb_platform.h`, host tri-state in `adb_host_gpio.h` only).

See [`troubleshooting.md`](troubleshooting.md) § “BT mouse jumps”.

---

## 12. Bluetooth multi-device pairing (firmware 2.2.1+)

**Symptom:** Keyboard works; mouse dead; or adapter hangs when pairing Xbox/Stadia with keyboard and mouse already connected — especially on **Mac cold boot**.

**Not an IIgs timing issue** — this is BT pairing + Mac global ADB reset interaction. Verbose UART (debug UF2) can mask or worsen timing races.

**Fix / guidance:**

- Flash **release** UF2 for pairing tests (`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`).
- Pair in order: **mouse → keyboard → gamepad** (Xbox/Stadia last).
- Firmware **2.2.1+** (`feature/BT-alignment`): defer Mac global ADB reset during BT setup; always-merge keyboard + gamepad key reports.

See [`bluetooth-pairing.md`](bluetooth-pairing.md) and [`troubleshooting.md`](troubleshooting.md) § Bluetooth.

---

## Suggested order to try

1. **(7)** Disable debug for testing  
2. **(8)** Compare USB vs BT  
3. **(9)** A/B test mouse accumulation ON/OFF (pointer smoothness)  
4. **(10)** A/B mouse SRQ suppression if you see BASIC slowdown  
5. **(2)** Bump Tlt minimum to ~160–200 µs  
6. **(4)** SRQ to 280–300 µs  
7. **(3)** Don’t overwrite `kbdreg0` too soon / simple queue  

The oscilloscope will then help confirm whether the issue is our response timing (Tlt, bit timing) or the host missing our reply.
