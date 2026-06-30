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

## Related docs

- [`iigs-debugging.md`](iigs-debugging.md) — IIgs keyboard/mouse timing experiments
- [`adb-passthrough-hub.md`](adb-passthrough-hub.md) — hub + chained trackball SRQ behaviour
- [`adb-host-mode.md`](adb-host-mode.md) — host mode setup and architecture
- [`release-notes.md`](release-notes.md) — version history (2.1.0 GPIO split)
