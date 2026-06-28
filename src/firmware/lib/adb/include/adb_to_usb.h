#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void adb_to_usb_init(void);
void adb_to_usb_keyboard_reg0(uint16_t reg0);
void adb_to_usb_mouse_reg0(uint16_t reg0);

#ifdef __cplusplus
}
#endif
