# Bluetooth pairing and memory configuration

This document summarizes findings from comparing HIDHopper ADB with two similar adapters that use Bluepad32 on Pico W: **amigahid-pico** and **ultramegausb-atari-st-rpikbd** (Atari). It covers Core 1 pause timing, BTstack/Bluepad32 config, and memory/XIP settings that affect second-device pairing stability.

---

## 1. Core 1 pause timing (Amiga vs Atari vs HIDHopper)

### 1.1 Delay *after* calling pause (discovery / connected)

When a HID device is discovered or connected, Core 1 is paused so that BTstack (Core 0) can use flash for NVM/bond storage without contention. The question is whether Core 0 waits *after* setting the pause flag before continuing with the connection.

| Project     | Delay after `pause_core1()` in `on_device_discovered` | Delay after `pause_core1()` in `on_device_connected` |
|------------|--------------------------------------------------------|-----------------------------------------------------|
| **Amiga**  | **No**                                                 | **No**                                             |
| **Atari**  | **No**                                                 | **No**                                             |
| **HIDHopper** | **Yes: 50 ms** (to let Core 1 enter paused loop before connection/NVM) | **Yes: 50 ms** |

**Conclusion:** Amiga and Atari do **not** add a delay after pausing Core 1. HIDHopper adds 50 ms in both callbacks to give Core 1 time to observe the flag and stop before the stack proceeds with pairing and the NVM write that follows “Pairing complete, success”. If second-device or first-attempt hangs persist, this delay can be tuned or removed for comparison.

### 1.4 Known behaviour: occasional first-attempt failure

On some power-ups or when pairing the first device, the adapter may reach **“Pairing complete, success”** and then stop during **“Requesting device information”** (GATT discovery). The device never reaches “device ready” and the adapter may need a reset. A second or third pairing attempt (often after power-cycling the peripheral or retrying) usually succeeds. This is more likely when flash/NVM is busy (e.g. first bond write after boot). If it happens, put the keyboard or mouse back into pairing mode and try again; no firmware change is required.

### 1.2 Delay *before* or *after* resume

| Project     | When Core 1 is resumed | Delay around resume |
|------------|-------------------------|----------------------|
| **Amiga**  | In `on_device_ready` (after device type–specific handling) | **50 µs** *inside* `amiga_quad_mouse_resume_core1()` after clearing the flag. 50 ms or 10 ms *before* resume for Xbox/Stadia vs other gamepads. |
| **Atari**  | In `on_device_ready` for Xbox/Stadia gamepads only       | **10 ms** `sleep_ms(10)` *before* calling resume. No delay inside resume. |
| **HIDHopper** | In `on_device_ready` for all device types              | **200 ms** `sleep_ms(200)` *before* calling resume (longer than Amiga’s 50 ms to reduce second-device pairing hangs). |

### 1.3 Core 1 behaviour while paused

| Project     | Paused-loop implementation |
|------------|-----------------------------|
| **Amiga**  | `busy_wait_us(5000)` then `continue` (no sleep). |
| **Atari**  | `busy_wait_us(1000)` then `continue` (no sleep). |
| **HIDHopper** | `sleep_ms(1)` then `continue` so Core 1 yields and the `flash_safe_execute` lockout IRQ can run when Core 0 does NVM write. |

**Recommendation:** If reverting toward Amiga/Atari behaviour for comparison, the paused loop could use `busy_wait_us(1000)` or `busy_wait_us(5000)` again; keeping `sleep_ms(1)` is intended to help the lockout handshake complete during second-device bond storage.

---

## 2. BTstack and Bluepad32 config alignment

### 2.1 `btstack_config.h` (ACL buffers, NVM, SM)

HIDHopper is aligned with amigahid-pico for multi-device (keyboard + mouse + gamepad):

| Setting                         | Amiga | Atari | HIDHopper |
|---------------------------------|-------|-------|-----------|
| `MAX_NR_CONTROLLER_ACL_BUFFERS` | 4     | 3     | 4         |
| `HCI_HOST_ACL_PACKET_NUM`       | 4     | 3     | 4         |
| `MAX_NR_SM_LOOKUP_ENTRIES`      | 3     | 3     | **4** (extra headroom for 2+ devices) |
| `NVM_NUM_DEVICE_DB_ENTRIES`     | 16    | 16    | 16        |
| `NVM_NUM_LINK_KEYS`             | 16    | 16    | 16        |
| `MAX_NR_GATT_CLIENTS` / `MAX_NR_HIDS_CLIENTS` / `MAX_NR_HCI_CONNECTIONS` | 4 | 4 | 4 |

HIDHopper uses 4 ACL buffers and 4 host ACL packets (like Amiga) for keyboard + mouse + gamepad. `MAX_NR_SM_LOOKUP_ENTRIES` was increased from 3 to 4 so the Security Manager has enough lookup entries when the second device pairs (identity resolving + new bond).

### 2.2 `sdkconfig.h` (Bluepad32)

All three use the same Bluepad32 limits: `CONFIG_BLUEPAD32_MAX_DEVICES 4`, `CONFIG_BLUEPAD32_MAX_ALLOWLIST 4`, `CONFIG_BLUEPAD32_GAP_SECURITY 1`, `CONFIG_BLUEPAD32_ENABLE_BLE_BY_DEFAULT 1`, `CONFIG_BLUEPAD32_PLATFORM_CUSTOM`, and the same log level.

---

## 3. Memory and XIP configuration

### 3.1 Binary type (XIP vs copy_to_ram)

| Project     | When BT enabled      | When BT disabled   | Set in CMake? |
|------------|----------------------|--------------------|---------------|
| **Atari**  | `default` (XIP)      | `copy_to_ram`      | **Yes** – `pico_set_binary_type(atari_ikbd default)` / `copy_to_ram` in root `CMakeLists.txt`. |
| **Amiga**  | (not set → SDK default) | (not set)       | No           |
| **HIDHopper** | (not set → SDK default) | (not set)     | No           |

SDK default when nothing is set is **`default`** (XIP). So all three run from flash (XIP) for the Bluetooth builds. Atari explicitly sets the type; Amiga and HIDHopper rely on the default.

### 3.2 Bluetooth bonding storage (TLV / flash vs RAM)

| Project     | TLV / link keys / LE device DB | Flash writes for bonding? |
|------------|----------------------------------|----------------------------|
| **Atari**  | **Custom** `btstack_cyw43.c`: **no-op TLV** + **in-memory link key DB**; LE device DB configured with no-op and early returns to avoid flash. | **No** – no flash used for BT bonding; bonds are lost on power cycle. |
| **Amiga**  | Standard SDK: `btstack_tlv_flash_bank`, TLV link keys, `le_device_db_tlv` on flash. | **Yes** – uses flash TLV and `flash_safe_execute`. |
| **HIDHopper** | Standard SDK: same as Amiga. | **Yes** – uses flash TLV. |

Atari’s comment in their custom `btstack_cyw43.c`: *“Use no-op TLV to avoid flash writes that can freeze Core 1; even with PICO_FLASH_ASSUME_CORE1_SAFE, cache invalidation can cause stalls.”* So Atari avoids second-device NVM/flash entirely; Amiga and HIDHopper rely on flash TLV and core coordination.

### 3.3 Implications for second-device pairing hang

- The hang often occurs after **“Connection encrypted: 1”** with no **“Pairing complete”** or **“Identity created”** for the second device. That is when the stack would write the new bond via TLV (flash).
- **HIDHopper** (and Amiga): Core 0 uses `flash_safe_execute()` for that write; Core 1 must participate in the lockout. A 25 ms delay after pause gives Core 1 time to enter the paused loop; `sleep_ms(1)` in the paused loop lets the lockout IRQ run.
- **Atari**: No bond is written to flash, so there is no NVM write during pairing and no flash_safe handshake. Pairing stability is achieved by avoiding flash use for BT, at the cost of non-persistent bonds.

**Optional directions if hangs continue:**

- Try **copy_to_ram** for the Pico W build (code runs from RAM; no execution from flash during pairing). Atari does *not* use copy_to_ram for BT; they use XIP and no flash for bonds.
- Consider an **Atari-style option**: in-memory link key DB and no-op or RAM-only LE device DB so bonding is not persisted but pairing no longer touches flash (larger change).
- Mark Core 1’s tight loop (and any code that runs while “paused”) with **`__not_in_flash_func`** so it runs from RAM and may reduce cache invalidation stalls when Core 0 writes flash.

---

## 4. Flash-safe execute and Core 1 init

All three call **`flash_safe_execute_core_init()`** from Core 1’s entry (before the main loop) so that when Core 0 (BTstack) calls `flash_safe_execute()` for TLV writes, the multicore lockout can complete. HIDHopper does this in `bt_pairing_sync_core1_init()` from Core 1’s `core1_main()`. Without this, Core 1 can block or the lockout can time out when the second device’s bond is stored.

---

## 5. Reference paths

- **Amiga:** `/Users/rich/Documents/Code/3rd party/amigahid-pico`  
  - `src/btstack_config.h`, `src/sdkconfig.h`, `src/bluepad32_platform.c`, `src/platform/amiga/quad_mouse.c`
- **Atari:** `/Users/rich/Documents/Code/Pico/Atari-Keyboard/ultramegausb-atari-st-rpikbd`  
  - `src/btstack_config.h`, `src/sdkconfig.h`, `src/bluepad32_platform.c`, `src/main.cpp`, `pico-sdk/src/rp2_common/pico_cyw43_driver/btstack_cyw43.c`
- **HIDHopper:** `src/firmware/src/btstack_config.h`, `src/firmware/src/sdkconfig.h`, `src/firmware/src/bluepad32_platform.c`, `src/firmware/src/bt_pairing_sync.c`, `src/firmware/lib/QuokkADB/src/quokkadb.cpp`

---

*Document created from findings during investigation of second-device (e.g. keyboard after mouse) pairing hangs. No code changes are prescribed; options are listed as recommendations.*
