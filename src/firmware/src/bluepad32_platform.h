/**
 * bluepad32 custom platform API for HIDHopper ADB (keyboard + mouse only)
 */

#ifndef _BLUEPAD32_PLATFORM_H
#define _BLUEPAD32_PLATFORM_H

#if ENABLE_BLUEPAD32

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Get keyboard data for index (0 .. MAX_BT_KEYBOARDS-1). out_keyboard must match uni_keyboard_t layout.
// Returns true if connected and has new data; clears updated flag.
bool bluepad32_get_keyboard(int idx, void* out_keyboard);

// Get count of connected Bluetooth keyboards
int bluepad32_get_keyboard_count(void);

// Get mouse data for index (0 .. MAX_BT_MICE-1). out_mouse must match uni_mouse_t layout.
// Returns true if connected and has new data; clears updated flag.
bool bluepad32_get_mouse(int idx, void* out_mouse);

// Get count of connected Bluetooth mice
int bluepad32_get_mouse_count(void);

// Get gamepad data for index (0 .. MAX_BT_GAMEPADS-1). out_gamepad must match uni_gamepad_t layout.
// Returns true if connected and has new data; clears updated flag.
bool bluepad32_get_gamepad(int idx, void* out_gamepad);

// Get count of connected Bluetooth gamepads
int bluepad32_get_gamepad_count(void);

// Delete all stored Bluetooth pairing keys
void bluepad32_delete_pairing_keys(void);

// Get device name for display. device_type: 'K' keyboard, 'M' mouse, 'G' gamepad. idx 0-based.
const char* bluepad32_get_device_name(char device_type, int idx);

// Platform entry (used by bluepad32_init.c)
struct uni_platform* get_my_platform(void);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_BLUEPAD32

#endif // _BLUEPAD32_PLATFORM_H
