/**
 * Bluetooth HID bridge - feed Bluepad32 keyboard/mouse/gamepad into ADB parsers.
 * Gamepad: D-pad/buttons → HID keyboard; left stick → mouse deltas (see docs/gamepad-support.md).
 * process_bluepad32_devices() is a no-op when ENABLE_BLUEPAD32 is 0.
 */
#pragma once

void process_bluepad32_devices(void);
