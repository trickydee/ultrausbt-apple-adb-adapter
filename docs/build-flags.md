# Firmware build flags (CMake)

Configure from the **project root** (or any directory); firmware sources live under `src/firmware`.

**Example:**

```bash
cmake -B build-pico2_w -S src/firmware -DPICO_BOARD=pico2_w
cmake --build build-pico2_w -j$(nproc)
```

`./build_all.sh` only passes **`-DPICO_BOARD=<board>`** for release builds; all ADB-related options below use the **defaults** in `src/firmware/CMakeLists.txt` unless you add more `-D` arguments.

---

## Board selection (Pico SDK)

| Variable | Values used here | Purpose |
|----------|------------------|---------|
| `PICO_BOARD` | `pico`, `pico_w`, `pico2`, `pico2_w` | Selects RP2040 vs RP2350 and whether **CYW43** (Wi‑Fi/BT) is present. **Required** for a correct link script and peripherals. Wireless boards enable **Bluepad32** at configure time. |

This is a **Pico SDK** setting, not defined in our `CMakeLists.txt`, but every build must set it.

---

## ADB and input behavior

| CMake flag | Kind | Default | Explanation |
|------------|------|---------|-------------|
| `ADB_DEBUG` | `option` | `OFF` | When `ON`, enables ADB protocol/timing **UART logging** (`adb.cpp` / related). Helps debugging; adds overhead. **`build_all.sh`** turns this **ON** only for `build-*-debug` directories. |
| `ADB_MOUSE_ACCUMULATE_DELTAS` | `option` | `ON` | **`ON`:** add incoming USB/BLE mouse `dx`/`dy` between ADB polls (with saturation). **`OFF`:** only the latest delta is kept (legacy). Improves smoothness when many small reports arrive between host polls. |
| `ADB_ATTENTION_LO_MIN_US` | `CACHE STRING` (integer µs) | `500` | Minimum **attention low** duration (µs) accepted in `ReceiveCommand`. Table 6-8 allows ~560–1040 µs on IIgs; `500` is a practical floor with measurement slack. Change for A/B tuning (e.g. `450`). Wired into the **`adb`** library as `ADB_ATTENTION_LO_MIN_US`. |
| `ADB_STRICT_DUTY_CYCLE_DECODE` | `option` | `OFF` | **`ON`:** decode received bits using **35% / 65%** low-time thresholds (IIgs-style). **`OFF`:** **midpoint** decode (more tolerant). Strict mode has regressed on some hosts; default stays off. |
| `ADB_STRICT_SYNC_WINDOW` | `option` | `OFF` | **`ON`:** sync window **42–91 µs** (IIgs envelope). **`OFF`:** tolerant **40–95 µs** window. Used in `ReceiveCommand` sync check. |
| `ADB_IIGS_MOUSE_SUPPRESS_SRQ` | `option` | `ON` | **`ON`:** mouse does **not** extend the SRQ line; keyboard SRQ only. Reduces IIgs **BASIC slowdown** when the mouse moves. **`OFF`:** legacy behavior (mouse can participate in SRQ). Set `OFF` only if you need to compare or hit an edge case. |

---

## How flags reach the code

- **Global:** `BT_USB_ADB_ADAPTER_VERSION_STRING` comes from `project(... VERSION ...)` in `CMakeLists.txt` (not a `-D` flag).
- **Top-level:** `ADB_DEBUG`, `ADB_IIGS_MOUSE_SUPPRESS_SRQ` → `add_compile_definitions` where applicable.
- **Library targets:**
  - **`adb`:** `ADB_ATTENTION_LO_MIN_US`, `ADB_STRICT_DUTY_CYCLE_DECODE`, `ADB_STRICT_SYNC_WINDOW` (`src/firmware/lib/adb/src/CMakeLists.txt`).
  - **`usb`:** `ADB_MOUSE_ACCUMULATE_DELTAS` (`src/firmware/lib/usb/src/CMakeLists.txt`).
  - **`QuokkADB`:** `ADB_IIGS_MOUSE_SUPPRESS_SRQ` (`src/firmware/lib/QuokkADB/src/CMakeLists.txt`).
- **C++ baseline:** `-DQUOKKADB` and `-DSCQ_RP2040_MUTEX` are set on `CMAKE_CXX_FLAGS` in `CMakeLists.txt` (not user-tunable options).
- **Wireless only (`pico_w` / `pico2_w`):** `ENABLE_BLUEPAD32` and CYW43 pin definitions are applied automatically; not separate ADB knobs.

---

## Related scripts

| Script | Purpose |
|--------|---------|
| `build_all.sh` | All boards, release + debug (`ADB_DEBUG=ON` for debug only). |
| `scripts/build_attention_ab.sh` | A/B: `ADB_ATTENTION_LO_MIN_US` values. |
| `scripts/build_decode_ab.sh` | A/B: `ADB_STRICT_DUTY_CYCLE_DECODE` on/off. |
| `scripts/build_mouse_srq_ab.sh` | A/B: `ADB_IIGS_MOUSE_SUPPRESS_SRQ` on/off (plus fixed decode/sync defaults). |

---

## See also

- `docs/iigs-debugging.md` — suggested order for trying options on IIgs.
- `docs/align-iigs-support-to-hardware-reference.md` — how options map to the IIgs hardware reference.
- `docs/release-notes.md` — when defaults changed.
