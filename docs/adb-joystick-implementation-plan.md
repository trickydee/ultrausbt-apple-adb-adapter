# ADB Joystick Implementation Plan (Concrete)

This plan implements one ADB joystick at address 0x04 using the Microsoft Sidewinder 56-bit packet format, fed by Bluepad32 gamepad data. Reference: `docs/adb-joystick-bluepad32-mapping.md` and `docs/ms-sidewinder-adb-packet.md`.

---

## Phase 1: Sidewinder packet packing

### 1.1 Add `sidewinder_pack` module (new files)

**Path:** `src/firmware/src/sidewinder_pack.c` and `src/firmware/src/sidewinder_pack.h` (or under `lib/adb` if preferred; keeping in `src` keeps ADB-agnostic packing separate).

**`sidewinder_pack.h`:**
```c
#if ENABLE_BLUEPAD32
#include "controller/uni_gamepad.h"

#define SIDEWINDER_PACKET_SIZE 7

// Pack uni_gamepad_t into 7-byte Sidewinder ADB packet (buffer must be SIDEWINDER_PACKET_SIZE).
void sidewinder_pack_from_gamepad(const void* gamepad, uint8_t* buffer);
#endif
```

**`sidewinder_pack.c`:**
- Implement `sidewinder_pack_from_gamepad(uni_gamepad_t* gp, uint8_t* buffer)`.
- Bit layout (LSB first, byte 0 = bits 0–7, byte 1 = 8–15, …):
  - Bits 0–3: base buttons (4 bits), 1 = released, 0 = pressed. Map e.g. bit0 = base bottom-left (e.g. X), bit1 = bottom-right (B), bit2 = top-right (Y), bit3 = top-left (A).
  - Bits 4–13: X axis 10-bit. Clamp `axis_x` to [-512,511], map to [0, 0x3FF]; center ≈ 0x1FF.
  - Bits 14–23: Y axis 10-bit. Same from `axis_y`; invert so “up” = low if doc says 0 = up.
  - Bits 24–27: hat 4-bit. 0 = off; from dpad: up=1, up+right=2, right=3, down+right=4, down=5, down+left=6, left=7, up+left=8.
  - Bits 28–30: 000 (reserved).
  - Bits 31–39: rudder 9-bit. From `axis_rx` (or throttle−brake), map to [0, 0x1FF], center ≈ 0xFF.
  - Bits 40–43: 4 trigger buttons (1=off, 0=pressed). Map e.g. main=RT/B, top=RB, side top=LB, side bottom=LT/A (adjust to taste).
  - Bits 44–45: 00.
  - Bits 46–55: throttle 10-bit. From `throttle` (0–1023) scale to [0, 0x3FF]; if unavailable use 0x1FF.
- Write bits into `buffer[0..6]` (byte 0 = bits 0–7, etc.). Use little-endian bit packing within bytes.

**Build:** Add `sidewinder_pack.c` to `src/firmware/src/CMakeLists.txt` (only when `ENABLE_BLUEPAD32`), and include path to Bluepad32 controller headers.

---

## Phase 2: Bluepad32 gamepad storage and API

### 2.1 `src/firmware/src/bluepad32_platform.c`

- **Storage:** Add `#define MAX_BT_GAMEPADS 1` (or 2 if you want a second slot later). Add:
  ```c
  typedef struct {
      uni_gamepad_t gamepad;
      bool connected;
      bool updated;
      char name[32];
  } bt_gamepad_storage_t;
  static bt_gamepad_storage_t bt_gamepads[MAX_BT_GAMEPADS];
  static uni_hid_device_t* gamepad_device_map[MAX_BT_GAMEPADS];
  ```
- **Device lifecycle:** In `on_device_connected`, when `ctl->klass == UNI_CONTROLLER_CLASS_GAMEPAD`, call a `find_slot`-style helper for gamepads and store device in `gamepad_device_map`. In `on_device_disconnected`, clear the slot for gamepad. In `on_device_ready`, if class is gamepad, set `bt_gamepads[slot].connected = true` and copy name (reuse pattern from keyboard/mouse).
- **Data callback:** In `my_platform_on_controller_data`, add:
  ```c
  case UNI_CONTROLLER_CLASS_GAMEPAD: {
      bt_gamepad_storage_t* storage = get_gamepad_storage(d);  // implement similar to get_keyboard_storage
      if (storage) {
          if (!storage->connected) { storage->connected = true; /* set name if empty */ }
          storage->gamepad = ctl->gamepad;
          storage->updated = true;
      }
      break;
  }
  ```
- **Getters:** Implement `bluepad32_get_gamepad(int idx, void* out_gamepad)` (return true if `bt_gamepads[idx].connected && bt_gamepads[idx].updated`, copy `uni_gamepad_t`, clear `updated`) and `bluepad32_get_gamepad_count(void)` (count connected).

### 2.2 `src/firmware/src/bluepad32_platform.h`

- Declare:
  ```c
  bool bluepad32_get_gamepad(int idx, void* out_gamepad);
  int bluepad32_get_gamepad_count(void);
  ```
- (Optional) Add `bluepad32_get_device_name(..., 'G', idx)` support for “Gamepad” name in BT device list if you show gamepads there.

---

## Phase 3: ADB game device (address 0x04)

### 3.1 `src/firmware/lib/adb/include/adb.h`

- Add:
  ```c
  #define GAME_DEFAULT_ADDR       0x04
  #define GAME_DEFAULT_HANDLER_ID 0x05   // or value from Mac driver research
  extern uint8_t game_addr;
  ```
- Declare (or keep in .cpp): `extern uint8_t gamepending` and `extern uint8_t gamesrq` (or reuse a single “gamepad pending” flag).

### 3.2 `src/firmware/lib/adb/src/adb.cpp`

- **Globals:** Add `uint8_t game_addr = GAME_DEFAULT_ADDR;`, `uint8_t game_handler_id = GAME_DEFAULT_HANDLER_ID;`, `uint8_t gamepending = 0;`, `uint8_t gamesrq = 0;`. Add buffer for Sidewinder packet: e.g. `uint8_t game_reg0_1[4];` (first 4 bytes) and `uint8_t game_reg2[2];` (bytes 4–5) and optionally byte 6 in a 7-byte `game_joystick_packet[7]` (preferred: one 7-byte buffer, then split for registers).
- **Reset:** In `Reset()`, set `game_addr = GAME_DEFAULT_ADDR`, `game_handler_id = GAME_DEFAULT_HANDLER_ID`, `gamepending = 0`, `gamesrq = 0`.
- **ProcessCommand:** After the keyboard block (after `else { if (kbdpending) kbdsrq = 1; }`), add a new block:
  ```c
  if (((cmd >> 4) & 0x0F) == game_addr) {
      switch (cmd & 0x0F) {
          case 0x1:  /* FLUSH */ break;
          case 0x8:  /* LISTEN R0 */ break;
          case 0x9:  /* LISTEN R1 */ break;
          case 0xA:  /* LISTEN R2 */ break;
          case 0xB:  /* LISTEN R3 */: read 16-bit register; handle address/handler change (like mouse); break;
          case 0xC:  /* TALK R0 */: if (gamepending) { Send16bitRegister(*(uint16_t*)&game_joystick_packet[0]); gamepending=0; gamesrq=0; } else { gamesrq=1; } break;
          case 0xD:  /* TALK R1 */: Send16bitRegister(*(uint16_t*)&game_joystick_packet[2]); break;
          case 0xE:  /* TALK R2 */: Send16bitRegister(*(uint16_t*)&game_joystick_packet[4]); break;  // or (packet[5]<<8)|packet[4] and add byte 6 if needed
          case 0xF:  /* TALK R3 */: Send16bitRegister(GetAdbRegister3Game()); break;
          default: break;
      }
  } else {
      if (gamepending) gamesrq = 1;
  }
  ```
- **GetAdbRegister3Game():** Add function similar to `GetAdbRegister3Mouse()`: build 16-bit reg3 with device address (e.g. `game_addr`) and `game_handler_id`, return it.
- **SRQ:** In `ReceiveCommand(srq)` call site (in main loop), pass `gamesrq` in the SRQ mask so the host can poll the joystick when data is pending (e.g. `adb.ReceiveCommand(mousesrq | kbdsrq | gamesrq)`).

**Register 2 packing note:** Sidewinder has 7 bytes. Use bytes 0–1 → R0, 2–3 → R1, 4–5 → R2. Byte 6 (throttle low 8 bits) can be combined with byte 5 in R2 as high byte if a driver expects 4×16-bit; for 3×16-bit, you can put the last 8 bits in the high byte of the third 16-bit word (so R2 = byte4 | (byte5<<8) and send one more byte somewhere, or drop byte 6 if the driver only uses 48 bits). Document choice in code; start with R0/R1/R2 = bytes 0–1, 2–3, 4–5 and add R2 high byte = byte 6 if needed.

**Buffer ownership:** The 7-byte joystick packet `game_joystick_packet[7]` is defined in `adb.cpp` and declared `extern` (e.g. in `adb.h` or a shared header) so the main loop can write it after calling `sidewinder_pack_from_gamepad`.

---

## Phase 4: Main loop — feed gamepad into ADB

### 4.1 `src/firmware/lib/QuokkADB/src/quokkadb.cpp`

- **Includes:** When `ENABLE_BLUEPAD32`, include `bluepad32_platform.h` and `sidewinder_pack.h` (or the header that declares the pack function).
- **Externs:** Add `extern uint8_t game_addr;`, `extern uint8_t gamepending;`, `extern uint8_t gamesrq;`, and `extern uint8_t game_joystick_packet[7];` (or whatever the adb layer exposes for the 7-byte buffer).
- **Loop (after `process_bluepad32_devices()`):** If `bluepad32_get_gamepad(0, &gp)` returns true, call `sidewinder_pack_from_gamepad(&gp, game_joystick_packet)` (or pass buffer from adb layer via a setter), then set `gamepending = 1`.
- **ReceiveCommand:** Change to `adb.ReceiveCommand(mousesrq | kbdsrq | gamesrq)`.
- **display_set_adb_status:** Pass `game_addr` (or 0 when no gamepad connected) for the `game_id` parameter so the display shows “G4” when the joystick is active. Use “gamepad connected” when `bluepad32_get_gamepad_count() > 0`; pass 0 when no gamepad so “G0” or “G-” can mean “no game device” (or keep G# only when a gamepad is connected and pass 0 otherwise).

---

## Phase 5: Display

### 5.1 `src/firmware/src/display/display.c` / `display.h`

- No signature change needed if `display_set_adb_status(..., game_id)` already exists; the main loop will pass `game_addr` when a gamepad is connected and 0 when not (or you can pass `game_addr` always and show “G4” even when no gamepad — then “G4” means “device at 4 is present”). Prefer: pass 0 when `bluepad32_get_gamepad_count() == 0`, and `game_addr` when > 0 so “G4” only appears when a gamepad is connected.

---

## Phase 6: Build and conditional compile

### 6.1 `src/firmware/CMakeLists.txt`

- Add `sidewinder_pack.c` to `HIDHopper-firmware` sources only when `ENABLE_BLUEPAD32` is ON (same as other Bluepad32-only sources).
- Ensure include path for `controller/uni_gamepad.h` is available when building `sidewinder_pack.c`.

### 6.2 Guarding joystick code in adb.cpp

- Wrap all game_addr / gamepending / gamesrq / Talk R0–R3 for game in `#if ENABLE_BLUEPAD32`. When Bluepad32 is off, no joystick device; `game_addr` can be undefined or a no-op so the rest of the code still compiles.

---

## File change summary

| File | Action |
|------|--------|
| `docs/adb-joystick-bluepad32-mapping.md` | Created (summary). |
| `docs/adb-joystick-implementation-plan.md` | Created (this plan). |
| `src/firmware/src/sidewinder_pack.c` | New: pack `uni_gamepad_t` → 7-byte Sidewinder. |
| `src/firmware/src/sidewinder_pack.h` | New: declare pack function. |
| `src/firmware/src/bluepad32_platform.c` | Add gamepad storage, on_controller_data gamepad case, get_gamepad/count. |
| `src/firmware/src/bluepad32_platform.h` | Declare get_gamepad, get_gamepad_count. |
| `src/firmware/lib/adb/include/adb.h` | Add GAME_DEFAULT_ADDR, GAME_DEFAULT_HANDLER_ID, extern game_addr (and gamesrq if exposed). |
| `src/firmware/lib/adb/src/adb.cpp` | Add game_addr, game_handler_id, gamepending, gamesrq, game_joystick_packet[7], Reset updates, ProcessCommand game block, GetAdbRegister3Game, SRQ mask. |
| `src/firmware/lib/QuokkADB/src/quokkadb.cpp` | Include/platform get gamepad; call pack; set gamepending; pass gamesrq to ReceiveCommand; pass game_id to display. |
| `src/firmware/src/CMakeLists.txt` | Add sidewinder_pack.c when ENABLE_BLUEPAD32. |

---

## Testing order

1. Build with `ENABLE_BLUEPAD32` (Pico W or Pico 2 W); confirm no link/compile errors.
2. Unit-check Sidewinder packing: call `sidewinder_pack_from_gamepad` with a fixed `uni_gamepad_t` (e.g. center axes, one button pressed) and assert expected bytes.
3. Connect a Bluepad32 gamepad; confirm `bluepad32_get_gamepad_count()` becomes 1 and `bluepad32_get_gamepad(0, &gp)` returns true with updated data.
4. On Mac (or ADB host), poll address 0x04 Talk R0/R1/R2 (and R3 for reg3); confirm data changes with stick/buttons; try a Sidewinder-compatible driver or test app.
5. Confirm display shows “G4” when a gamepad is connected and “G0” or no G when disconnected (per chosen convention).

---

## Optional follow-ups

- **Deadzone:** In `sidewinder_pack_from_gamepad`, treat axis values in [-threshold, +threshold] as center (0x1FF for 10-bit) to avoid jitter.
- **Button mapping:** Make base/trigger button mapping configurable (e.g. compile-time or flash) so different controllers map sensibly.
- **Second gamepad:** Increase `MAX_BT_GAMEPADS` to 2 and map second gamepad to a second ADB address (e.g. 0x05) if the host supports multiple joysticks.
