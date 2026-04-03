# Project TODO

This document tracks upcoming development work for HIDHopper ADB.

## 1) Add Gravis Flightstick support (Bluetooth + USB)

### Goal
Support Gravis Flightstick-style joystick input devices and map their controls to useful output actions for vintage hosts.

### Scope
- Support USB HID joystick/gamepad input path.
- Support BLE controller input path (via Bluepad32).
- Add a common internal joystick event model so USB and BLE share the same mapping logic.
- Expose a configurable mapping profile for axes/buttons.

### Implementation tasks
- Identify target devices to support first:
  - Gravis Flightstick hardware-compatible USB adapters.
  - BLE gamepads that can emulate similar axis/button layouts.
- Define a normalized joystick data model:
  - Axes: X/Y, optional throttle/rudder.
  - Buttons: trigger and additional action buttons.
  - Hat/D-pad as optional directional source.
- Implement USB parser support:
  - Detect joystick usage pages/collections.
  - Parse report descriptors and extract axis/button state.
  - Add filtering and dead-zone handling for noisy analog axes.
- Implement BLE parser support:
  - Reuse Bluepad32 controller state.
  - Map Bluepad32 fields into the same normalized joystick model.
- Add output mapping layer:
  - Decide default behavior (for example: joystick -> keyboard events, mouse movement, or custom ADB device behavior).
  - Make mapping profile compile-time configurable first, runtime configurable later.
- Add rate control:
  - Clamp update rate to avoid flooding ADB host with excessive event traffic.
  - Reuse existing motion accumulation ideas where appropriate.
- Add build flags and docs:
  - `JOYSTICK_SUPPORT` / mapping profile flags in CMake.
  - Document supported devices and default mappings in `docs/iigs-debugging.md` or a new joystick doc.

### Validation tasks
- Verify USB joystick detection, pairing, and stable input on Pico 2 W.
- Verify BLE controller behavior and reconnect reliability.
- Validate host behavior on Apple IIgs and Mac Quadra for latency and stability.
- Confirm no regression in keyboard/mouse performance while joystick input is active.

## 2) Reimplement adbmon functionality and design an adbmon diagnostic board

### Goal
Create a Pico-native ADB monitor/diagnostic tool that captures ADB traffic with timing details and provides practical hardware for repeatable troubleshooting.

### Scope
- Reimplement adbmon as a modern Pico project (not Arduino/Uno code).
- Capture and decode ADB command/data transactions and timing windows.
- Produce serial/USB logs suitable for regression comparisons.
- Define a small diagnostic board/interposer design for safe inline bus monitoring.

### Firmware implementation tasks (adbmon-pico)
- Create `src/adbmon-pico` (or similar) as an independent build target.
- Implement passive ADB line capture:
  - Timestamp transitions using RP2040/RP2350 timers or PIO-assisted capture.
  - Decode attention, sync, bit cells, stop, and optional SRQ extension.
- Build decoder modes:
  - Raw edge/timing dump mode.
  - Decoded frame mode (Talk/Listen/Reset/Flush, address, register, payload).
  - Optional strict/lenient decode modes to mirror firmware A/B settings.
- Add logging transport:
  - USB CDC serial output (primary).
  - Optional UART mirror for external capture.
- Add ring-buffered tracing:
  - Prevent log printing from perturbing capture timing.
  - Emit dropped-event counters and buffer high-water marks.
- Add replay/fixture hooks:
  - Save trace snippets for test cases.
  - Use sample captures to verify decoder correctness offline.

### Diagnostic board design tasks
- Define board role:
  - Inline interposer between host and ADB device.
  - Passive monitor mode as baseline.
  - Optional active injection mode only if explicitly needed later.
- Electrical requirements:
  - Preserve ADB signal integrity (high impedance monitor input).
  - Proper pull-up/pull-down strategy consistent with ADB bus rules.
  - ESD/basic protection and robust connector orientation.
- Connectors and usability:
  - ADB in/out connectors with clear labels.
  - Status LEDs for power/activity/error.
  - Test pads for logic analyzer probing.
- Power strategy:
  - Decide whether board is bus-powered, USB-powered, or both with isolation/protection.
- Rev A deliverables:
  - Schematic + PCB + BOM.
  - Bring-up checklist and validation procedure.

### Validation tasks
- Compare monitor decode output against known-good ADB traces.
- Validate timing measurements against IIgs hardware reference limits.
- Confirm monitor operation does not alter host/device behavior.
- Document known caveats and minimum sampling/capture requirements.

## 3) Add absolute positioning support for USB tablets mapped to Wacom

### Goal
Support USB graphics tablets (digitizers) that report **absolute** X/Y (and typically pressure/tilt), and drive the host using **Apple ADB Wacom-style** semantics so classic Mac drivers can treat the device like a real Wacom tablet.

### Reference material
- Captured on-wire / register layout notes: `docs/wacom.md` (hardware capture from a KT-0405-A; use as a guide, validate against Apple/Wacom documentation for your target OS).

### Scope
- USB HID: tablet/digitizer usage (not the current relative mouse path).
- Map logical tablet coordinates to the ADB tablet protocol (handler IDs, multi-byte register 0 / extended formats as required).
- Optional: stylus buttons, pressure, and tool proximity — phased by feasibility.
- Out of scope for an initial milestone: BLE tablets (can follow once USB path is solid).

### Implementation tasks
- Add a tablet HID report parser (report ID, absolute X/Y, pressure where present).
- Implement coordinate scaling/mapping from USB logical range to ADB Wacom ranges.
- Extend ADB device model beyond relative `mousereg0` (see current mouse path in `adbmouseparser.cpp`) with tablet-specific register packing per `docs/wacom.md` and Apple docs.
- Handle ADB Talk/Listen sequences tablets expect (may differ from keyboard/mouse).
- Add CMake feature flag(s) and document build/runtime limits.
- Document supported tablet models and fallbacks (e.g. degrade to relative mouse) if needed.

### Validation tasks
- Verify on a representative 68k/PowerPC Mac with Wacom driver stack.
- Cross-check behavior against a real Wacom ADB tablet where possible.
- Confirm no regression for standard keyboard/mouse when tablet support is disabled.

## Suggested order
1. Build `adbmon-pico` capture MVP first (highest leverage for all future debugging).
2. Use monitor data to guide joystick mapping/rate-limit decisions.
3. Finalize diagnostic board Rev A after firmware capture model is stable.
4. Tablet/Wacom absolute positioning after relative mouse/keyboard path remains stable (large feature; benefits from adbmon traces).
