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

---

## 7. Debug / Serial

**Idea:** With `global_debug` on, `Serial.print` can block and delay the next ADB poll.

**Parameter to consider:** Use a non-debug build (or turn debug off) for normal typing tests so Serial isn’t in the path.

---

## 8. Bluetooth vs USB keyboard

**Idea:** If the source is a Bluetooth keyboard, Bluepad32 and radio traffic can add jitter and delay the main loop.

**Parameter to consider:** For comparison, try a USB keyboard and see if “fast typing” is more reliable; that would point to main-loop/polling or BT load.

---

## Suggested order to try

1. **(7)** Disable debug for testing  
2. **(8)** Compare USB vs BT  
3. **(2)** Bump Tlt minimum to ~160–200 µs  
4. **(4)** SRQ to 280–300 µs  
5. **(3)** Don’t overwrite `kbdreg0` too soon / simple queue  

The oscilloscope will then help confirm whether the issue is our response timing (Tlt, bit timing) or the host missing our reply.
