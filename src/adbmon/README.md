# adbmon — Pico ADB bus monitor

Passive Apple Desktop Bus (ADB) traffic decoder for Raspberry Pi Pico / Pico 2 on the **ultramegausb DIY board** (see [`../docs/hardware.md`](../docs/hardware.md)).

## What it does

- Snoops the ADB **DATA** line (GPIO 19) without driving the bus
- Decodes Attention → command byte → optional 16-bit payload (Talk / Listen)
- Detects **SRQ** stop-bit extension and **global reset** (≥ 2.8 ms attention low)
- Logs to **USB CDC** and **UART** (GPIO 0 / Pico **pin 1** @ 115200 — same as adapter debug):

```
ADB evt=Talk raw=3C addr=3 reg=0 srq=1 data=- t_us=52141041
ADB evt=Talk raw=FC addr=F reg=0 srq=0 data=2:8081 t_us=52152932
# adbmon checkpoint id=1 capture=ON t_us=52160000
```

### BOOTSEL / board button controls

| Action | Effect |
|--------|--------|
| **Short tap** | Insert `# adbmon checkpoint id=N …` marker (LED blinks once) |
| **Hold ~0.8 s** | Toggle capture **ON/OFF** — prints `# adbmon capture-start` or `capture-stop` (2 or 3 LED blinks) |

Use **GPIO 7** (QuokkADB OLED middle button) if your board has the UI header — it is more reliable than BOOTSEL during heavy logging. BOOTSEL on the Pico module also works (sampled at low rate so USB CDC stays alive).

Capture defaults to **ON** at boot. Use checkpoints to bracket H1/H2/H3 clips in the serial log without stopping the bus decode loop.

## Build

Requires [Pico SDK](https://github.com/raspberrypi/pico-sdk) (1.5+ or 2.x).

From the **repo root** (recommended — uses shared SDK cache and copies to `dist/`):

```bash
./build_all.sh
# → dist/adbmon-pico.uf2, dist/adbmon-pico2_w.uf2 (default boards)
# → dist/BT-USB-ADB-Adapter-firmware-*.uf2 (adapter firmware)
```

Skip adbmon when you only need adapter firmware:

```bash
BUILD_ADBMON=0 ./build_all.sh
# or
./build_all.sh --no-adbmon
```

Build **adbmon only** (fast iteration — does not build adapter firmware):

```bash
./build_all.sh --adbmon-only
ADBMON_BOARDS="pico" ./build_all.sh --adbmon-only
# → dist/adbmon-pico.uf2
```

`ADBMON_BOARDS` only selects which boards get adbmon UF2s; use `--adbmon-only` to skip adapter builds.

**adbmon only** (manual CMake):

```bash
source scripts/lib/build_common.sh
ensure_pico_sdk
cmake_build_adbmon_dir build-adbmon-pico -DPICO_BOARD=pico
# → build-adbmon-pico/adbmon.uf2
```

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cd src/adbmon
mkdir -p build && cd build
cmake .. -DPICO_BOARD=pico
make -j
```

Artifacts: `adbmon.uf2`, `adbmon.elf` (under the build directory).

### Options

| CMake flag | Default | Purpose |
|------------|---------|---------|
| `ADBMON_STRICT_DECODE` | OFF | IIgs 35%/65% duty + 42–91 µs sync windows |
| `ADB_ATTENTION_LO_MIN_US` | 500 | Minimum attention low time |

## Flash and use

1. Hold BOOTSEL, plug USB, copy `adbmon.uf2` to the drive.
2. Connect the Pico on QuokkADB hardware to an **inline** ADB chain (host → adapter ADB port → downstream devices).
3. Open serial at 115200 (USB CDC device or UART TX on GPIO 0 / Pico pin 1).

**Important:** This firmware does **not** emulate keyboard or mouse. Use it only when you want to observe bus traffic. For normal adapter use, flash the main QuokkADB firmware from `src/firmware`.

## Layout

| Path | Role |
|------|------|
| `src/main.cpp` | Main loop, stats |
| `src/adbmon_bus.cpp` | Passive decode (aligned with QuokkADB `adb.cpp`) |
| `src/adbmon_log.cpp` | Ring-buffered trace output |
| `legacy/` | Original Arduino Uno `adbmon` (PlatformIO) |

## Roadmap

See [`docs/adbmon.md`](../docs/adbmon.md) and [`docs/FUTURE_WORK.md`](../docs/FUTURE_WORK.md) §5.

- [ ] Multi-byte Talk payloads (Gravis 7-byte register 0)
- [ ] Raw edge / timing dump mode
- [ ] OLED live view (shared UI with main firmware)
- [ ] Adapter-integrated monitor mode (runtime toggle)
- [ ] Diagnostic interposer PCB
