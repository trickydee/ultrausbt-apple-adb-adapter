#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_hid_send_keyboard(uint8_t modifier, const uint8_t keycodes[6]);
void usb_hid_send_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel);
void usb_hid_device_task(void);

#ifdef __cplusplus
}
#endif
