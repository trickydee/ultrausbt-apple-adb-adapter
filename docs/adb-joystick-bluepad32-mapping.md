# ADB Joystick Support: Review and Bluepad32 Mapping

## 1. Microsoft Sidewinder ADB Packet (Reference)

- **56-bit (7-byte) packet**, one logical “report” per poll.
- **Layout (bit ranges, LSB first in bytes):**
  - **4 base buttons** (1=released, 0=pressed): base bottom-left, bottom-right, top-right, top-left.
  - **X axis**: 10 bits, 0 = left, 0x3FF = right.
  - **Y axis**: 10 bits, 0 = up, 0x3FF = down.
  - **Hat**: 4 bits, 0 = off, 1 = up, 2 = up-left, 3 = left, 4 = down-left, 5 = down, 6 = down-right, 7 = left, 8 = up-right.
  - **Rudder**: 9 bits, 0 = counter-clockwise, 0x1FF = clockwise.
  - **4 trigger buttons**: side bottom, side top, top trigger, main trigger (same 1=off/0=on convention).
  - **Throttle**: 10 bits, 0 = up (idle), 0x3FF = down (full).

So we need a function that, given axes and buttons, fills a 7-byte buffer in this bit layout. ADB then exposes this as one or more 16-bit registers.

See also: `docs/ms-sidewinder-adb-packet.md`.

---

## 2. Thrustmaster Driver (Mac OS, USB Side)

- **Role**: Mac driver for **USB** devices (e.g. iMate ADB→USB + Thrustmaster). It does **not** define the ADB wire protocol; it defines how the **OS** sees the joystick (HID report descriptor and 10-byte HID report).
- **Useful takeaway**: Axis/button semantics (X, Y, throttle, rudder, hat, triggers) and that classic Mac joystick software often expects **0–255** (or 0–1023) axes and a **hat** (e.g. 0 = null, 1–8 = directions). So our ADB format should match what existing Mac joystick code expects (e.g. Sidewinder-style).
- **Not needed for firmware**: The iMate (or similar) does ADB↔USB; we only need to **speak ADB** in a format compatible with Sidewinder-style drivers (e.g. 56-bit packet).

---

## 3. ADB Bus in HIDHopper Today

- **Keyboard**: address 0x02, handler 0x02; register 0 (and 2) for key state.
- **Mouse**: address 0x03, handler 0x01; register 0 for buttons + X/Y.
- **Joystick**: not implemented. Display already has “G#” (game_id) but it’s always 0; no third device on the bus.

So we need a **third ADB device**: “game/joystick” at a dedicated address (e.g. **0x04**), with its own Talk/Listen handling and a register that returns the Sidewinder-format packet (or equivalent).

---

## 4. Bluepad32 Gamepad Data (`uni_gamepad_t`)

From `uni_gamepad.h` and the platform code:

- **D-pad**: `dpad` (bitfield: up/down/left/right).
- **Axes**: `axis_x`, `axis_y`, `axis_rx`, `axis_ry` (e.g. about -512..511).
- **Sim**: `brake`, `throttle` (0–1023).
- **Buttons**: `buttons` (A, B, X, Y, shoulder L/R, trigger L/R, thumb L/R).
- **Misc**: `misc_buttons` (system, select, start, capture).

The HIDHopper platform only handles **keyboard** and **mouse** in `on_controller_data`; **gamepad** is ignored. So step one is to start handling `UNI_CONTROLLER_CLASS_GAMEPAD` and storing the latest gamepad state for the ADB joystick.

---

## 5. Recommended Approach: Bluepad32 → ADB Joystick

### 5.1 Choose an ADB Format

- **Recommendation**: Implement the **Sidewinder 56-bit packet** from the reference doc. It’s documented, and many Mac joystick drivers support it (or something close). If you later need a second format (e.g. Thrustmaster-style), you can add another handler ID and a different packing function.

### 5.2 ADB Device: One Joystick at 0x04

- Add a **joystick/game device** at address **0x04** (or another free address; 0x04 is common for “game” devices).
- Use a **handler ID** that matches what Mac Sidewinder drivers expect (you may need to check driver source or docs; often 0x05 or similar for “game/joystick”). If unknown, start with one value and make it configurable later.
- **Registers**: The 56-bit packet fits in **3 × 16-bit** (reg 0, 1, 2) or **4 × 16-bit** (with padding). Recommend:
  - **Talk Register 0**: bytes 0–1 of the 7-byte packet.
  - **Talk Register 1**: bytes 2–3.
  - **Talk Register 2**: bytes 4–5.
  - Byte 6 (last 8 bits of throttle) can go in the high byte of reg 2 (with low byte of reg 2 being byte 5), so 3 registers are enough. Alternatively use a 4th register and put byte 6 there for clarity; Mac driver will tell you which it expects if you have a reference.

### 5.3 Bluepad32 → Sidewinder Mapping (Concrete)

- **Base buttons (4)**  
  Map from gamepad buttons, e.g.:  
  - Bottom-left → e.g. X or shoulder L  
  - Bottom-right → e.g. B or shoulder R  
  - Top-right → e.g. Y or thumb R  
  - Top-left → e.g. A or thumb L  
  (Choose a mapping that fits “flight stick” semantics; can be made configurable later.)

- **X axis (10-bit, 0 left → 0x3FF right)**  
  - `axis_x`: clamp to [-512, 511], then map linearly to [0, 0x3FF] (center ≈ 0x1FF).

- **Y axis (10-bit, 0 up → 0x3FF down)**  
  - `axis_y`: same as X; invert if the doc says “up” = low value (so “down” = high).

- **Hat (4-bit, 0 off, 1–8 directions)**  
  - From `dpad`: 0 → 0 (off). Up only → 1; Up+Right → 2; Right → 3; Down+Right → 4; Down → 5; Down+Left → 6; Left → 7; Up+Left → 8. Match the doc’s order (1=up, 2=upleft, … 8=upright) exactly.

- **Rudder (9-bit)**  
  - Use `axis_rx` (twist) or, if absent, `(throttle - brake)` or one of the triggers. Map from current range to 0..0x1FF (center ≈ 0xFF).

- **Trigger buttons (4)**  
  - Map from e.g. main trigger = A or RT, top = B, side top/bottom = shoulder L/R or similar. Use same 1=released, 0=pressed as base buttons.

- **Throttle (10-bit, 0 up → 0x3FF down)**  
  - Use `throttle` (0–1023); scale to 0..0x3FF. If no throttle, use a default (e.g. 0x1FF) or derive from triggers.

Implement one function that takes `uni_gamepad_t*` and writes the 7-byte Sidewinder packet; then have ADB Talk handlers return reg 0/1/2 (and optionally 3) from that buffer.

### 5.4 Thrustmaster Driver (Optional Second Format)

- The Thrustmaster driver’s **USB** report is 10 bytes (axes + buttons + hat). That’s for the **OS**, not for ADB.
- If you later want “Thrustmaster-style” on ADB, you’d need a separate ADB packet format (and possibly handler ID) and a second packing function; the same `uni_gamepad_t` can feed both. Start with Sidewinder only.

---

## 6. Implementation Order (High Level)

1. **Pack Sidewinder 56-bit packet** from `uni_gamepad_t` (one function, unit-testable with fixed inputs).
2. **Bluepad32**: handle gamepad in `on_controller_data`, store latest gamepad; add `bluepad32_get_gamepad` / count.
3. **ADB**: add game device at 0x04, Talk R0/R1/R2 (and R3 if needed) from the 7-byte buffer; set gamepad SRQ when updated.
4. **Main loop**: each tick, if gamepad updated, pack into Sidewinder buffer and mark pending.
5. **Display**: show game address in “G#”.
6. **Tune**: axis scaling, deadzone, and button mapping for common controllers; optionally make mapping configurable (e.g. in flash or compile-time).

---

See also: `docs/adb-joystick-implementation-plan.md` for the concrete file-by-file implementation plan, and `docs/supported-gamepads.md` for recommended controllers (e.g. PlayStation DualSense, Stadia).
