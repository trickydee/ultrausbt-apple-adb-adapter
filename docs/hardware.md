# Hardware — ultramegausb Apple ADB Adapter

KiCad project: **apple-adb.kicad_sch** (rev 1.1, ultramegausb.com).

Custom dual-port ADB pass-through board with BSS138 level shifting.

## Board overview

| Feature | Schematic ref | Description |
|---------|---------------|-------------|
| **ADB ports** | J1, J2 | Two Mini-DIN-4 sockets, wired in parallel (shared DATA, +5 V, GND) |
| **MCU** | U1 | Raspberry Pi Pico |
| **Level shifter** | Uc1 (`Conn_LLC_AD8`) | 4-channel BSS138 module between Pico 3.3 V and ADB 5 V DATA |
| **DATA pull-up** | R2 (`ADB-PU1`) | Pull-up from ADB DATA to +5 V |
| **Pico power from bus** | D2 (`1N5817`) | ADB +5 V → Pico VSYS. May be **bridged** on bench builds so USB +5 V feeds the bus |
| **Bus power switch** | SW_ADB-Pwr | ADB port +5 V feed |
| **Debug header** | Debug1 | GND, **GP0** TX, **GP1** RX |
| **Display header** | Display1 | **GP2–GP9** (wire I2C/buttons per `display_config.h`) |

## GPIO map (firmware ↔ schematic)

| GPIO | Use | Firmware macro |
|------|-----|----------------|
| **18** | ADB DATA TX (Pico pin 24) | `ADB_OUT_GPIO` |
| **19** | ADB DATA RX (Pico pin 25) | `ADB_IN_GPIO` |
| **25** | Status LED D1 via R1 | `LED_GPIO` |
| **0** | Debug UART TX | `UART_TX_GPIO` |
| **1** | Debug UART RX | `UART_RX_GPIO` |
| **2–9** | Display1 header | (optional OLED / buttons) |

Defined in `src/firmware/lib/QuokkADB/include/quokkadb_gpio.h`.

### TX / RX swap

If host enumeration still fails after confirming GPIO 18/19, swap `ADB_OUT_GPIO` and `ADB_IN_GPIO` in `quokkadb_gpio.h` in case the BSS138 channels are reversed on the module.

## ADB DATA path

```
J1/J2 pin 1 (DATA) ──┬── R2 pull-up ── +5 V
                     ├── Uc1 (BSS138) HV side
                     │       LV ── GP18 (OUT), GP19 (IN)
                     └── both ports (common bus)
```

## Power

| Mode | Path |
|------|------|
| **Device mode** (Mac is host) | Mac +5 V on J1/J2 → D2 → Pico VSYS |
| **Host mode bench test** | Pico USB +5 V → (bridged D2) → ADB +5 V; **SW_ADB-Pwr** on; Mac **disconnected** |

On-board **R2** provides the DATA pull-up — the Mac is not required for that when running as host.

## Dual-port pass-through

```
[Mac or host] ──► [J1] ═══ shared DATA / +5 V / GND ═══ [J2] ──► [trackball / keyboard]
                              │
                         GP18 / GP19 via Uc1
```

See [`adb-passthrough-hub.md`](adb-passthrough-hub.md) for hub-mode firmware.

## Usage cautions

- **No hot-plug on ADB** — connect or disconnect from the vintage Mac only when the Mac is **off**.
- **One bus master at a time** — Mac off and unplugged when Pico runs ADB host mode.

## Related docs

- [`troubleshooting.md`](troubleshooting.md) — BT mouse regression, host timing, flash images
- [`adb-passthrough-hub.md`](adb-passthrough-hub.md)
- [`adb-host-mode.md`](adb-host-mode.md)
- [`changes.md`](changes.md)
- [`led-support.md`](led-support.md)
