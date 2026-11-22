# Apple ADB native bonded BLE reconnect test — 2.3.1

The adapter integration is committed as `3868b1e` on
`feature/bluepad32-logi-mx-investigations`. The default build now uses the published
reconnect implementation rebased onto upstream develop `ec42eb1` through the pinned
[trickydee fork](https://github.com/trickydee/bluepad32). The original reconnect
proposal is [Bluepad32 PR #236](https://github.com/ricardoquesada/bluepad32/pull/236).
The pre-existing edit to `docs/release-notes.md` remains unrelated and uncommitted.

## Sources and hardware

- Bluepad32 submodule: `src/firmware/bluepad32`, exact published revision
  `06aed0361eb094f9ca6b6029da88ef60a200af95`, based on upstream `ec42eb1`.
  `branch = codex/ble-reconnect-develop-20261122` records the fork branch; normal submodule
  updates still use the committed gitlink, not the moving branch tip.
  No external checkout or local reconnect patch is required.
- This replaces reconnect pin `f7e9448`. Upstream removed
  `uni_bt_conn_get_address`; the two adapter calls now copy `d->conn.btaddr` with
  `memcpy(addr, d->conn.btaddr, sizeof(addr))`. The obsolete host-test stub was
  removed. Adapter behavior and SDK selection are otherwise unchanged.
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

Run from the repository root, with `BLUEPAD32_SOURCE_DIR` unset:

```sh
git submodule update --init --recursive
./build-all.sh
python3 tests/test_bluepad32_host.py
```

For existing clones, run `git submodule sync --recursive` first so the Bluepad32
remote follows `.gitmodules`. New recursive clones use the fork automatically.

Verification used fresh build trees and retained them for inspection:

```sh
env -u BLUEPAD32_SOURCE_DIR PICO_SDK_PATH="$PWD/.pico-sdk/pico-sdk" BUILD_RELEASE_DIR=build-reconnect-20261122-release BUILD_DEBUG_DIR=build-reconnect-20261122-debug BUILD_ADBMON_DIR=build-reconnect-20261122-adbmon BUILD_KEEP_DIRS=1 ./build-all.sh > /tmp/apple-adb-reconnect-20261122-build.log 2>&1
```

`BLUEPAD32_SOURCE_DIR=/absolute/path/to/bluepad32` remains an optional development
override; use the same published revision for comparable results. The build helper
explicitly selects the pinned submodule when no override is set, even when reusing
an external-source build tree. Direct CMake also defaults to the submodule in a new
build directory; an existing CMake cache retains its selection unless overridden.
The C-only SDK compatibility shim applies to both paths. Configuration failures
propagate, and failed builds cannot replace dist artifacts.

## Upstream follow-up

Once PR #236 is merged, validate a replacement upstream commit with `build-all.sh`,
the host tests, and the reconnect/ADB hardware checklist below. Then change the
Bluepad32 submodule URL back to `https://github.com/ricardoquesada/bluepad32.git`,
remove the temporary fork branch setting, and commit the validated replacement
gitlink. Do not switch solely because the PR has merged.

## Hardware test procedure

1. Use the Pico 2 W adapter debug UF2. Capture its existing UART on GP0 TX / GND at
   115200 baud. Verify version 2.3.1 and the native bonded reconnect test marker.
2. In ADB Device mode, pair the Logitech MX keyboard/mouse using normal pairing.
   Confirm keyboard typing, mouse movement/buttons, held-button dragging, and existing
   OLED device labels.
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

## Rollout hardware status — 2026-11-22

The user validated the new upstream-plus-reconnect implementation on Amiga:
cold boot, Logitech power cycles, MX keyboard and mouse, held-button dragging,
and controller coexistence. This is supporting cross-adapter evidence, not an
Apple ADB hardware result. **Apple hardware validation for `06aed03` is pending**;
run the checklist above with the new Apple artifacts, including ADB Host mode
and USB regressions. No Apple hardware was flashed during this rollout.

## User-reported hardware result — 2026-10-06

The user reported that the Apple ADB adapter looks to be working and authorized
committing the 2.3.1 integration. This records overall user-reported hardware success;
it does not establish that every checklist item above was individually exercised
or passed. Detailed per-scenario results remain unconfirmed. No physical tests were
performed by the agent. Do not clear bonds between reconnect trials.

## Earlier integration verification

The original 2.3.1 integration was built successfully with the local Bluepad32
revision `28ac7526840f32c7ec8cb63bff8c6281d49ad498`. Its callback host check accepted
empty/null names with CoD=0 and preserved gamepad pause behavior. A mocked configure
failure stopped the build helper before a stale build. These historical checks
are separate from the published-pin verification below and the user-reported
hardware result above.

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

## Previous published-pin verification — `f7e9448`

- `build-all.sh` exited 0 from fresh build trees: release and UART-debug Pico 2 W
  adapters plus Pico adbmon. No compiler warnings or errors were found in
  `/tmp/apple-adb-fork-pin-build.log`.
- Both adapter caches select the pinned submodule, version 2.3.1, and the expected
  debug OFF/ON setting. The SDK compatibility include is present in Bluepad32 C
  flags and absent from assembly flags. No CMake changes were needed for this pin.
  The optional external path uses the same target and compatibility settings;
  this verification built the default submodule path, not the external override.
- Both adapter binaries contain the reconnect UART marker. All three UF2s have
  valid block magic/alignment, and dist copies match their fresh build outputs.
- `python3 tests/test_bluepad32_host.py` passed. Shell syntax checks passed.
- The shared Bluepad32 review checkout and cached SDK remain clean and unchanged.
  Unrelated release-note edits were verified byte-for-byte unchanged.
- No hardware was flashed for this published-pin verification. Reconnect/sleep-wake,
  held-button dragging, bond/name retention, and ADB/USB regressions still require
  hardware validation; the earlier user report is not a test of this exact pin.

### Previous UF2 artifacts — `f7e9448`

- `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2`: 1382912 bytes; SHA-256
  `5014fcbee1270cecdfca13d5eb83806e7839d61a409bc9518ee7f8fc952ec681`.
- `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host-debug.uf2`: 1382912 bytes; SHA-256
  `c43400987d03a30492434203298c4598d0aeecf1907b63bc832a3791dea76190`.
- `dist/adbmon-pico.uf2`: 83968 bytes; SHA-256
  `b9ec67cbbefd7f713dca5803c2691f9e52bf300c40557308c664fe0f1398481a`.

## Rollout build verification — 2026-11-22, `06aed03`

- Fresh `build-all.sh` completed with exit 0 for Pico 2 W release and UART-debug
  adapters and Pico adbmon. No compiler warnings or errors were found in
  `/tmp/apple-adb-reconnect-20261122-build.log`.
- Both adapter caches select the pinned Apple submodule and existing SDK 2.2.0,
  with debug OFF/ON respectively. Both binaries contain the 2.3.1 reconnect marker.
  No SDK or CMake compatibility changes were required.
- `python3 tests/test_bluepad32_host.py` passed after removing the obsolete address
  helper stub, exercising the actual adapter code with its direct address copies.
- All three UF2s have valid block alignment and magic; dist copies equal fresh
  build outputs. SDK HEAD remains `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` and clean.
  The Apple Bluepad32 submodule is clean at the requested pin. Unrelated
  `docs/release-notes.md` edits are byte-for-byte unchanged.
- Apple hardware validation remains pending. The Amiga results above do not
  establish Apple reconnect, ADB timing, or physical bond/name retention behavior.

### Current rollout artifacts

- `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host.uf2`: 1396736 bytes; SHA-256
  `c88f108ec7028de4120064f0ab2957d4f02f552f56d74b76168939b79789d7f1`.
- `dist/ultrausbt-Apple-ADB-adapter-firmware-pico2_w-host-debug.uf2`: 1396736 bytes; SHA-256
  `6fa4359d1ac6b39ca359b8d90e4296c93e7ec5af9d64ffb07b1cc09b53f0e7e3`.
- `dist/adbmon-pico.uf2`: 83968 bytes; SHA-256
  `719ce218be29ab0b9f6d825d10545caf80f61ec69203d08d6d0e1e8021061bbe`.
