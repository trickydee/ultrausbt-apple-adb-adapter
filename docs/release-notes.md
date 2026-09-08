# ultrausbt-Apple-ADB-adapter – Release notes

Lineage: this firmware descends from QuokkADB and adbuino. Target hardware: [`hardware.md`](hardware.md).

## feature/gamepad

- **Bluetooth gamepad:** One BT gamepad slot in Bluepad32 (`uni_gamepad_t`, `bluepad32_get_gamepad` / `bluepad32_get_gamepad_count`). **Phase B shipped:** gamepad → **HID keyboard** (D-pad/buttons) and **left stick → mouse** via `bt_hid_bridge` / `KeyboardPrs` + `MousePrs` (see [gamepad-support.md](gamepad-support.md)). OLED **Devices** / **Bluetooth names:** BT gamepad **count**, **G1** name, and optional live **`BT GP:`** legend when a pad is connected.
- **Phase C (planned):** Native Gravis MouseStick II handler **0x23** — see [`FUTURE_WORK.md`](FUTURE_WORK.md) §2 / §11.

## 2.2.3

- **ADB Host typing:** Emit a USB HID report after each ADB Talk R0 key event (press+release in one poll no longer vanishes); queue keyboard reports for Core 1; HID `bInterval` **1 ms**; keyboard poll **8 ms**; defer LED Talk R2 while keys are flowing.

## 2.2.2

- **ADB host mode keyboard:** Fix **`a`** key missing — ADB keycode **`0x00`** is valid (Mac QWERTY `a`); host translator wrongly treated `0` as empty. Only **`0xFF`** (`ADB_REG_0_NO_KEY`) means no key.

## 2.2.1

- **Bluetooth pairing stability (Atari v22.1.0 / Amiga recipe):** Refcounted Core 1 pause in `bt_host_coop.c`; `__wfe()` pause loop on Core 1; `bt_callback_busy_wait_ms()` only in Bluepad32 callbacks (no `sleep_ms` during pairing); 30 ms discovery settle + 100 ms pre-resume; pause on BLE gamepad discovery only (CoD `0x0508`, Stadia/Xbox name — not generic `"gamepad"`); no double-pause on connect; 45 s pause watchdog; tunables in `bt_pairing_config.h`.
- **Multi-device BT input:** Always-merge keyboard + gamepad key reports via `bluepad32_peek_keyboard()` / `bluepad32_peek_gamepad()` before `KeyboardPrs.Parse()` — fixes keyboard queue flood when Xbox pairs before keyboard.
- **Mac boot + BT pairing:** Defer Mac global ADB reset while BT link is forming or within 2.5 s after `device_ready` (`bluepad32_bt_defer_adb_reset()`) — fixes mouse dead on ADB when pairing during Mac cold boot.
- **Xbox reconnect:** Force-release Core 1 pause on disconnect and key wipe; clear orphan slots on failed connect.
- **Docs:** [`bluetooth-pairing.md`](bluetooth-pairing.md), [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md), [`troubleshooting.md`](troubleshooting.md) § Bluetooth, [`FUTURE_WORK.md`](FUTURE_WORK.md) §1 marked done.

## 2.2.0

- **ADB host mode reliability:** Improved keyboard polling (12 ms), RX preamble (idle R0 no longer logged as errors), and probe logic (devices not dropped on idle `R0=0x0`).
- **Multi-device / passthrough:** Keyboard + trackball work on daisy-chain and dual-port setups; skip Mac-style trackball relocation when a pointing device is already at `@0x3`.
- **Locking caps lock:** Sync via ADB register 2 (not R0 `0x39`); USB HID LED output → Listen R2 for keyboard LED; caps-release R2 read pulses PC state when needed.
- **Docs:** [`troubleshooting.md`](troubleshooting.md) host-mode section; **`build-all.sh`** rename from `build_all.sh`.

## 2.1.0

- **Device/host GPIO split:** Restore pre-2.0 drive-high ADB GPIO in shared code for **ADB>Mac** device mode (fixes BT mouse jumps and collision detection). Host open-collector GPIO moved to `adb_host_gpio.h` only; host timing (765 µs attention, RX preamble) unchanged.
- **Docs:** [`adb-shared-gpio-rollback.md`](adb-shared-gpio-rollback.md), [`troubleshooting.md`](troubleshooting.md); **`build-all.sh`** simplified to Pico 2 W adapter + debug + adbmon.

## 2.0.0

- **ADB host mode (major):** New **ADB>USB** operating mode polls vintage ADB keyboards and pointing devices on the passthrough bus and presents them as a composite USB HID keyboard/mouse to a modern PC. Manual mode switch via OLED; persisted in flash.
- **Multi-device bus master:** Address-aware enumeration supports keyboard @0x2 plus multiple pointing devices (e.g. mouse @0x3, relocated trackball @0xF). Mac-style trackball relocation, periodic hot-plug rescan with bus reset, and QMK-aligned attention/timing.
- **OLED ADB Bus screen:** Shows configured vs working devices per address (`OK` / `--`), with splash summary in host mode.
- **adbmon:** Standalone bus monitor firmware and capture docs for comparing host behaviour against real Mac traces.
- **Hardware docs:** DIY board pinout and GPIO map (GP18/19 ADB, GP25 LED); HIDHopper references removed.

## 1.0.18

- **Bluetooth mouse drag:** Patch Bluepad32 `uni_hid_parser_mouse.c` to persist button state across movement-only BLE reports; `bt_hid_bridge.cpp` ORs a button latch into synthetic HID reports so drags do not spuriously release the button.
- **USB + BT mouse movement:** Restore the main-branch ADB register-0 snapshot model (`GetAdbRegister0()` on `MouseChanged()`); avoid servicing BLE during ADB bit timing, which had caused jerky movement and broken clicks.
- **Multi-BT mouse:** `bluepad32_peek_mouse()` and merged mouse processing retained; USB button state is no longer overwritten by Bluetooth sync.

## 1.0.17

- **Build speed:** `build_all.sh`, `./build.sh`, and `make` now share a **single** Pico SDK checkout under **`.pico-sdk/pico-sdk`** (and **picotool** under **`.pico-sdk`**) when **`PICO_SDK_PATH`** is not set—avoiding repeated SDK/picotool downloads for each `build-*` directory. See `scripts/lib/build_common.sh` and `./scripts/cmake_with_pico_sdk.sh`. **`./scripts/cleanup_build_artifacts.sh --include-sdk-cache`** removes **`.pico-sdk`** when you need a full reset.

## 1.0.16

- **Product rename:** CMake project/target **`ultrausbt-Apple-ADB-adapter-firmware`**, UF2 outputs (`ultrausbt-Apple-ADB-adapter-firmware*.uf2`), boot banner, Bluetooth device name, and top-level docs now use **ultrausbt-Apple-ADB-adapter**.

## 1.0.15

- **Repository cleanup for this fork:** The tree is focused on the **ultrausbt** DIY Apple ADB adapter firmware (USB + Bluetooth via Pico / Pico 2), not on legacy **ADBuino** Arduino hardware or third-party retail adapter PCBs.
- **Removed ADBuino-era firmware paths:** Unused ADBuino/PlatformIO Arduino target sources, the old `adbuino` CI workflow, and related utilities that only applied to that stack.
- **Removed third-party hardware-design artifacts:** Legacy case CAD, KiCad/gerber bundles, and extra product images not used by this firmware project. Pinout images useful for DIY wiring (`adb_pinout.png`, Pico pinout) are retained under `images/`.
- **Documentation layout:** User-facing docs live under `docs/` (including [`hardware.md`](hardware.md), `adb.md`, and notes such as `led-support.md`). The old top-level `doc/` folder is gone; `README` links updated accordingly.
- **License text:** `LICENSE` now describes this fork’s goals and third-party stack (including TinyUSB and Bluepad32).
- **Mouse SRQ suppression default:** `ADB_IIGS_MOUSE_SUPPRESS_SRQ` is now **ON** by default in CMake (improves IIgs BASIC and general behavior; use `-DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF` for legacy mouse SRQ). `build_all.sh` no longer passes this flag explicitly.

## 1.0.14

- **IIgs mouse SRQ suppression:** `build_all.sh` now enables `ADB_IIGS_MOUSE_SUPPRESS_SRQ=ON` so the mouse path does not extend SRQ on the IIgs. This prevents the BASIC loop slowdown when moving the mouse.
- **Validation:** confirmed improved behavior on both Apple IIgs (Taifun Boot) and an ADB Mac Quadra.

## 1.0.10

- **IIgs mouse movement tuning:** Added build-time option `ADB_MOUSE_ACCUMULATE_DELTAS` to control how mouse `dx/dy` is handled between ADB polls.
  - `ON` (default): accumulate deltas with saturation (reduces dropped micro-movement and pointer jerk on some IIgs apps).
  - `OFF`: legacy behavior (latest delta wins between polls).
- **IIgs attention floor tuning:** Added build-time setting `ADB_ATTENTION_LO_MIN_US` (default `500`) so attention timing can be A/B tested without source edits.
- **A/B test script:** Added `scripts/build_attention_ab.sh` to build two UF2s with identical firmware except attention floor (`500` vs `450`), now defaulting to `pico2_w` with `PICO_BOARD` override support.
- **Debugging docs:** Updated `docs/iigs-debugging.md` with mouse accumulation A/B guidance and exact CMake flags.
- **Cross-host validation:** The same IIgs-tuned firmware has been tested on an ADB Mac Quadra and showed improved behavior there as well, indicating the receive-path robustness changes are beneficial beyond IIgs-only scenarios.

## 1.0.7

- **Mouse wheel support:** Scroll wheel from USB and Bluetooth mice is now supported. ADB has no native wheel, so wheel is emulated as Up/Down arrow key presses (one key event per wheel tick). Works with both USB HID and Bluepad32 Bluetooth mice.

## 1.0.6

- **ADB disconnect display:** When the adapter is unplugged from the ADB bus, the splash now correctly shows `ADB: --` after about 2 seconds (connection state is only updated when a valid command is received from the bus).
- **Display refresh:** Splash no longer redraws on every SRQ or collision change, only when connection or device IDs change, avoiding full-screen I2C updates during mouse movement and improving responsiveness.
- **Build script:** `build_all.sh` now auto-sets the Pico SDK: it tries common paths (`~/pico/pico-sdk`, `~/pico-sdk`, etc.) and falls back to `PICO_SDK_FETCH_FROM_GIT=ON` if none are found.

## 1.0.5

- **ADB status line on splash (OLED):** When connected, the bottom line shows `ADB: K# M# G#` (keyboard, mouse, and gamepad device IDs). Two optional suffixes indicate bus state:
  - **S** — Service request (SRQ): keyboard or mouse has data pending and is requesting service from the host.
  - **!** — Collision: an ADB bus collision was detected.
  Example: `ADB: K2 M3 G0 S` means connected with kbd 2, mouse 3, no gamepad, and a device has an SRQ pending.

## 1.0.2

- **Right mouse button (ctrl-click) no longer hangs the device.**  
  When the right button was used in ctrl-click mode (default for compatibility with System 6 / System 7), the firmware could deadlock waiting for the keyboard queue to drain from inside the mouse report handler. The blocking waits were removed; Ctrl and click events are enqueued and sent by the main loop, so ctrl-click behaviour is unchanged and the device no longer hangs.

## 1.0.1

- Firmware version numbering: single source of truth in `CMakeLists.txt`; banner and identity show product name and version.
- Flash settings sector moved to avoid overlap with BTstack TLV region on wireless builds.

## 1.0.0

- **Supported boards:** Pico, Pico W, Pico 2, and Pico 2 W. Use `build_all.sh` to build firmware for all four.
- **Bluepad32 support for Bluetooth keyboards and mice** on Pico W and Pico 2 W (boards with CYW43). Bluetooth HID devices are discovered and paired via [Bluepad32](https://github.com/ricardoquesada/bluepad32); keyboard and mouse reports feed into the same ADB pipeline as USB, using the existing parsers and register handling. Up to 2 BT keyboards and 2 BT mice are supported (first of each is active). Build with `pico_w` or `pico2_w` to enable; default builds remain USB-only for non‑wireless boards.
