#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void adb_to_usb_init(void);
void adb_to_usb_keyboard_reg0(uint16_t reg0);
void adb_to_usb_mouse_reg0(uint16_t reg0);
void adb_to_usb_apply_reg2(uint16_t reg2);
void adb_to_usb_note_host_leds(uint8_t hid_leds);

#ifdef __cplusplus
}
#endif
