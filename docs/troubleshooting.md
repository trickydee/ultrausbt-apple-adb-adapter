# Troubleshooting

Common issues on the **ultramegausb** DIY ADB adapter. IIgs-specific tuning parameters live in [`iigs-debugging.md`](iigs-debugging.md). GPIO rollback history: [`adb-shared-gpio-rollback.md`](adb-shared-gpio-rollback.md).

---

## Build and flash

| Symptom | Check |
|---------|--------|
| Wrong firmware flashed | Use **`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`** from `./build-all.sh` — one image with **ADB → Mac** and **ADB → USB** (OLED toggle). Do not flash a legacy device-only UF2 unless you built it manually. |
| UART debug | Flash **`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host-debug.uf2`**; connect GPIO **0** (Pico pin 1) @ **115200**. |
| Bus traces | Flash **`dist/adbmon-pico.uf2`** (passive monitor — does not emulate keyboard/mouse). See [`src/adbmon/README.md`](../src/adbmon/README.md). |

---

## BT mouse jumps or stutters (ADB → Mac) — firmware 2.0.0 regression

**Symptom:** USB mouse fine; **Bluetooth** mouse movement occasionally **jumps** or sends wrong direction. Keyboard usually OK. Seen after **2.0.0** host-mode release when using the unified `-host` build in device mode.

**Cause (fixed in 2.1.0):** Shared ADB GPIO was changed to **tri-state** (open-collector release) for host development. Device mode **collision detection** in `adb_platform.cpp` assumes `data_hi()` **drives GP18 high** and checks `gpio_get(ADB_OUT_GPIO)` during TX. With tri-state, the pin reads low while the bus is high via R2 → false collisions → aborted `Send16bitRegister` → garbled mouse register 0.

**Fix (2.1.0+):** Shared `adb_platform.h` / `quokkadb_gpio.cpp` restored to **drive-high** for device mode. Host-only tri-state lives in `adb_host_gpio.h` (used only by `adb_host.cpp`).

**If it returns:** Flash **2.1.0** or later. With debug UF2, watch for `MOUSE: Collision on sending register 0`. Do **not** re-apply tri-state to shared platform code without a runtime mode split — see [`adb-shared-gpio-rollback.md`](adb-shared-gpio-rollback.md).

---

## Host mode — missed keys / `Tlt err=-1 t=0 IN=high`

**Symptom:** Fast typing drops keys; debug log alternates `RX Talk R0` OK with `RX fail (Tlt err=-1 t=0) IN=high`. Worse on `-host-debug` UF2.

**Cause:** RX preamble treated `wait_data_hi()` returning **0 µs** (bus already high) as failure. UART logging stretched timing and increased false failures.

**Fix (host-mode improvements):** ADB-spec RX preamble — wait for bus high after command, then low for device start bit. Keyboard poll **≥12 ms**; pointing devices **24 ms**; no blind poll of ghost mouse addresses; light rescan (no global reset) while devices are working.

---

## ADB host mode (ADB → USB) — polling fails or no devices

Host mode needs **timing** in `adb_host.cpp` (not the shared GPIO rollback above):

| Requirement | Value | Notes |
|-------------|-------|--------|
| Attention low | **765 µs** + `place_bit1()` start bit | **800 µs** total per ADB timing; not 800 µs gap + separate start |
| RX preamble | Bus high/low wait (ADB spec) | Before decoding device reply after Talk |
| TX GPIO (host) | Tri-state via `adb_host_gpio.h` | Release bus between command and RX |

**Symptom if wrong:** enumeration fails, `Tlt` / `start` / `sync` / `BIT` RX errors in UART debug, keyboard/mouse never appear on PC.

**Restore reference:** [`adb-shared-gpio-rollback.md`](adb-shared-gpio-rollback.md) § “If host mode breaks”; compare `adb_host.cpp` to commit `62611dc` or later.

**Setup:** Vintage Mac **off and disconnected**; power Pico from USB; ADB accessories on pass-through ports only. See [`hardware.md`](hardware.md) and [`adb-host-mode.md`](adb-host-mode.md).

---

## Host vs device GPIO — do not conflate fixes

| Change | Affects ADB → Mac? | Affects ADB → USB? |
|--------|-------------------|-------------------|
| 765 µs attention + RX preamble (`adb_host.cpp`) | No | **Yes** — required |
| Shared tri-state `data_hi()` (2.0.0) | **Yes** — broke collision | Marginal for TX; not the main host fix |
| Drive-high shared GPIO + `adb_host_gpio.h` (2.1.0) | **Yes** — restores device | Host keeps tri-state in host-only code |

---

## GPIO 18 / 19 swapped

If host enumeration or device replies are consistently wrong after wiring checks, swap `ADB_OUT_GPIO` and `ADB_IN_GPIO` in `quokkadb_gpio.h` (BSS138 channel order). See [`hardware.md`](hardware.md).

---

## Bluetooth — multi-device pairing

User guide: [`bluetooth-pairing.md`](bluetooth-pairing.md). Use the **release** UF2 from `./build-all.sh` (`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`) for pairing tests. The **debug** UF2 changes timing and can mask or trigger Heisenbugs. Full developer notes: [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md).

### Recommended pair order

| When | Order |
|------|--------|
| Mac **cold boot** (adapter and Mac power on together) | **Mouse → keyboard → gamepad** (Xbox/Stadia) |
| Mac already running | Same order; generally reliable |

Pairing **gamepad before keyboard** (especially Xbox BLE on Mac cold boot) was a common failure mode before firmware **2.2.1**.

### Mouse dead after pairing (Mac boot)

**Symptom:** UART shows all three BT devices “device ready”; keyboard works; **mouse does not move** on ADB.

**Cause:** Mac sends a **global ADB reset** (`ALL: Resetting devices`) during boot while BT enumeration is still in progress. `adb.Reset()` mid-pair relocates addresses and clears mouse state.

**Fix (2.2.1+):** `bluepad32_bt_defer_adb_reset()` holds the reset until BT link setup finishes and a **2.5 s** post-`device_ready` settle (`BT_POST_READY_ADB_SETTLE_MS`).

**Workaround (older firmware):** Reset the adapter after Mac has booted, then pair in order mouse → keyboard → gamepad.

### Keyboard queue flood / adapter hang (Xbox paired first)

**Symptom (debug UF2):** Repeated `unable to enqueue new KeyDown`; typing stops; device may feel hung.

**Cause:** Gamepad and keyboard shared `KeyboardPrs.Parse()` at fake BT address `0x80`. Gamepad-only reports caused spurious KeyUp/KeyDown when Xbox paired before the keyboard.

**Fix (2.2.1+):** `bluepad32_peek_keyboard()` / `bluepad32_peek_gamepad()` — always merge keyboard + gamepad key reports before `Parse()`. Gamepad keys are suppressed during Core 1 pause when a BT keyboard is connected.

**Workaround (older firmware):** Pair keyboard before gamepad, or clear pairings (Map Devices → **`˄+˯` 5 s**) and re-pair in recommended order.

### Xbox won’t reconnect after sleep

**Symptom:** `Failed to set device information client`, `Device cannot connect in time`; USB host or BT inputs stuck after failed reconnect.

**Cause:** Core 1 pause depth stuck > 0 after aborted pair or double discovery pause.

**Fix (2.2.1+):** `core1_force_release_bt_pause()` on disconnect and before key wipe; no second pause on `device_connected`; 45 s watchdog.

**Workaround:** Map Devices → **`˄+˯` 5 s** to clear pairings; power-cycle the Xbox controller; flash latest firmware.

### Xbox BLE vs PS5 (DualSense)

**Xbox / Stadia (BLE):** Triggers Core 1 pause on discovery (CoD `0x0508` or name match). Longer bond path; more sensitive to pair order and Mac boot timing.

**PS5 (BR/EDR):** Does **not** use the BLE gamepad pause path — pairing with mouse + keyboard is typically easier.

### Logitech “Identity resolving failed”

Often **non-fatal** on MX Keys / MX Master. If the device reaches “device ready” and works, ignore this line.

---

## Related docs

- [`iigs-debugging.md`](iigs-debugging.md) — IIgs keyboard/mouse timing experiments
- [`adb-passthrough-hub.md`](adb-passthrough-hub.md) — hub + chained trackball SRQ behaviour
- [`adb-host-mode.md`](adb-host-mode.md) — host mode setup and architecture
- [`release-notes.md`](release-notes.md) — version history (2.1.0 GPIO split, 2.2.1 BT pairing)
- [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md) — developer pairing port notes
- [`gamepad-support.md`](gamepad-support.md) — BT gamepad mapping and pairing cross-link
