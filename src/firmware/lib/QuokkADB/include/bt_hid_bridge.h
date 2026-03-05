/**
 * Bluetooth HID bridge - feed Bluepad32 keyboard/mouse into ADB parsers.
 * process_bluepad32_devices() is a no-op when ENABLE_BLUEPAD32 is 0.
 */
#pragma once

void process_bluepad32_devices(void);
