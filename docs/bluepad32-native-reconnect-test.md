# Apple ADB native bonded BLE reconnect test — 2.3.1

Test integration on `feature/bluepad32-logi-mx-investigations`, based on
local `main` `dbe05813878eeebe9330cf05b1778a24667929e2`. Local main matched the recorded
origin/main at implementation time. The user authorized committing this integration
on 2026-10-06; no fetch, merge, or push was performed.
The pre-existing edit to `docs/release-notes.md` is unrelated and preserved.

## Sources and hardware

- Shared Bluepad32: `/Users/rich/Documents/Code/Pico/bluepad32`, branch
  `codex/bonded-ble-reconnect`, exact clean revision
  `28ac7526840f32c7ec8cb63bff8c6281d49ad498`.
- Existing repository-cached Pico SDK **2.2.0** retained. Macro-only HIDS host/client
  compatibility is forced for C compilation of Bluepad32 only, never assembly.
- Adapter: stock **pico2_w (Pico 2 W / RP2350)**, main's default. No separate PCB
  revision selector is used by this build. GPIO, ADB timing, and hardware defaults unchanged.
- Normal build convention retained: unified device/host adapter, UART-debug adapter
  (`ADB_DEBUG=ON`), and passive **pico (RP2040)** adbmon. adbmon is not Bluetooth firmware
  and does not carry adapter version 2.3.1.
- Scanning/autoconnect and pairing remain enabled; no additional advertisement debug.
  UART marker: `2.3.1 native bonded reconnect test; persistent names + ready-only slots; scanning/pairing ON; advertisement debug OFF`.
- No adapter reconnect patch is applied. Ordinary discovery filtering remains in
  Bluepad32; its directed reconnect exception checks a stored bond and address type.
  The existing Apple ADB callback accepts empty name / CoD=0 without policy changes.
  Existing names, fallback labels, and storage are retained.

## Reproduce

Run from `/Users/rich/Documents/Code/Pico/Apple-ADB/ultramegausb-apple-adb`:

```sh
BLUEPAD32_SOURCE_DIR=/Users/rich/Documents/Code/Pico/bluepad32 BUILD_RELEASE_DIR=build-native-reconnect-release BUILD_DEBUG_DIR=build-native-reconnect-debug BUILD_ADBMON_DIR=build-native-reconnect-adbmon BUILD_KEEP_DIRS=1 ./build-all.sh > /tmp/apple-adb-native-reconnect-2.3.1-build.log 2>&1
```

The retained build trees from 2.3.0 were reused. The adapter sources were rebuilt
and linked as 2.3.1; marker and timestamps verified both new UF2s. Unchanged adbmon
passed its incremental build and retains the same hash. Retained CMake caches,
flags, source build rules, and binaries allow source-path and freshness verification. With no override, the build helper explicitly selects the pinned
`src/firmware/bluepad32` submodule, including when reusing an external-source build tree.
Direct CMake configuration also defaults to that submodule in a new build directory.
Configuration failures now propagate; failed builds cannot replace dist artifacts.

## Hardware test procedure

1. Use the Pico 2 W adapter debug UF2. Capture its existing UART on GP0 TX / GND at
   115200 baud. Verify version 2.3.1 and the native bonded reconnect test marker.
2. In ADB Device mode, pair the Logitech MX keyboard/mouse using normal pairing.
   Confirm keyboard typing, mouse movement/buttons, and existing OLED device labels.
   Existing bonds need one named pairing-mode connection to seed the new metadata;
   a previously stored bond alone cannot reconstruct a name that was never saved.
3. Power off/on each peripheral without clearing bonds or entering pairing mode.
   Confirm device-ready logging and restored ADB input. Repeat several times.
4. Let each peripheral sleep, then wake it. Repeat with both paired, and after
   restarting the adapter with bonds retained. Capture UART across each transition.
5. Check ordinary fresh pairing still works and an unknown directed advertiser
   without a stored bond is not admitted via the bonded exception. Record the model,
   connection slot, timings, logs, and any failure to reconnect.
6. Connect a gamepad after keyboard/mouse connections and disconnections; confirm
   its G1 name, D-pad/buttons and stick-to-mouse input. Apple ADB supports only one
   gamepad, so removing the first of two gamepads is not an applicable test. Instead
   pair two keyboards (then two mice), disconnect the first, and confirm the remaining
   device's name and input both appear at public index 0. Reconnect in reverse order.
   Regression-check USB keyboard/mouse, OLED mode switch, and ADB Host mode.
7. Reboot the adapter after named connections and confirm both bonds and restored
   keyboard/mouse names persist. Finally clear bonds through the existing UI, confirm
   metadata is cleared, and pair again to seed names anew.
8. Repeat reconnect checks with the release UF2 to exclude UART-debug timing effects.

## User-reported hardware result — 2026-10-06

The user reported that the Apple ADB adapter looks to be working and authorized
committing the 2.3.1 integration. This records overall user-reported hardware success;
it does not establish that every checklist item above was individually exercised
or passed. Detailed per-scenario results remain unconfirmed. No physical tests were
performed by the agent. Do not clear bonds between reconnect trials.

## Build verification results

- `build-all.sh` exited 0; release, debug, and adbmon all passed. No compiler
  warnings or errors in `/tmp/apple-adb-native-reconnect-2.3.1-build.log`.
- Both adapter CMake caches select the exact shared path, version 2.3.1,
  `pico2_w`, and `rp2350-arm-s`. Their Bluepad32 build rules compile
  `/Users/rich/Documents/Code/Pico/bluepad32/src/components/bluepad32/bt/uni_bt_le.c`.
  Include flags resolve to the shared checkout; the compatibility forced include
  appears in C flags and is absent from ASM flags. Debug option is OFF/ON respectively.
- Both linked adapter binaries contain the required UART marker. UF2 timestamps
  follow the edited firmware source; dist copies equal fresh build outputs. All
  UF2 blocks have valid start/end magic and 512-byte block alignment.
- Extracted adapter discovery callback host test passed for empty and null names
  with CoD=0, normal named discovery, and existing gamepad pause behavior (dependencies
  stubbed). This checks callback admission, not radio behavior.
- Mocked CMake configure failure stops the helper before attempting a stale build.
  `bash -n` and `git diff --check` passed.
- Shared HEAD rechecked at the exact SHA above, with clean status. Existing
  release-notes diff verified byte-for-byte unchanged. No legacy reconnect patch
  exists in the adapter build path or was added.
- No build blockers. Overall hardware success was reported by the user on
  2026-10-06 (above); individual reconnect, sleep/wake, and ADB regression checklist
  results remain unconfirmed. The agent did not flash hardware.

## Atari findings reviewed for Apple ADB

1. **Early flash workaround not applicable.** `quokkadb.cpp` launches Core 1 before
   `bluepad32_init()`. Core 1's first action is `flash_safe_execute_core_init()`;
   Bluetooth initialization also performs the existing radio reset delays before
   `uni_init()`. Final target flags include `LIB_PICO_MULTICORE=1`, and the link map
   resolves `flash_safe_execute` to the normal SDK `pico_flash/flash.c` object.
   No IRQ-only bypass, SDK patch, Core 1 ordering, GPIO, or protocol timing change
   was introduced. This is source/link evidence, not proof of physical bond retention.
2. **Slot issue partly applicable.** Disconnect already guarded allocating getters
   with `find_existing_slot`, so Atari's cross-class disconnect leak was absent.
   Report callbacks did still allocate (and keyboard/mouse reports could mark ready).
   Storage getters now explicitly choose allocation only at device-ready; reports
   and disconnects look up existing mappings. Disconnect releases mappings even
   when storage is not connected and drops pending names. Public get/peek/name APIs
   now share connected-device enumeration: losing storage slot 0 no longer hides
   slot 1 from the OLED while input still works. Existing one-gamepad limit retained.
3. **Persistent names applicable.** Previously names existed only in RAM/discovery
   cache. `bluepad32_names.c` reuses only Amiga's metadata implementation, with no
   legacy reconnect code. UBN TLV tags are keyed by BLE bond slot, versioned, and
   checked against identity address and address type. Missing/exact generic fallback
   names are restored; genuine current/discovery names win. Names are saved at ready
   after BLE security/service discovery. Unchanged records are not rewritten, and
   clear-pairing removes metadata and pending discovery names. Existing bonds need
   a named pairing-mode connection to seed names. Shared Bluepad32 and SDK untouched.

### Repeatable host tests

Run `python3 tests/test_bluepad32_host.py` from this repository. It compiles actual
adapter C bodies using stubbed SDK/radio types and services (real headers/ABI checked
by firmware builds). Tests passed for pre-ready reports, cross-class disconnect,
reserved-slot release, gamepad slot 0 input, keyboard and mouse gaps, saved-name
restore, unchanged-write suppression, identity address/type mismatch, missing bond,
classic protocol exclusion, corrupted records, simulated RAM reset with retained TLV,
genuine discovery-name preference, generic-name exclusion, and clear-bonds cleanup.
This does not test physical flash writes, Bluetooth radio behavior, or ADB timing.

### UF2 artifacts and SHA-256

- `/Users/rich/Documents/Code/Pico/Apple-ADB/ultramegausb-apple-adb/dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2`
  - SHA-256: `51069970e84333895d562f7831e8bd9170fc6e636fbb61be7ebdf38cc574bcd9`
  - Size: 1340928 bytes.
- `/Users/rich/Documents/Code/Pico/Apple-ADB/ultramegausb-apple-adb/dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host-debug.uf2`
  - SHA-256: `24d5cc7d4ab94dcee1cb45a2f53a63a0b13109299938d428c13b59a41f413ffe`
  - Size: 1340928 bytes.
- `/Users/rich/Documents/Code/Pico/Apple-ADB/ultramegausb-apple-adb/dist/adbmon-pico.uf2`
  - SHA-256: `f9af305277b89321e01bac49c28b94bdc726742a1027e26bff095d7901653526`
  - Size: 83968 bytes.
