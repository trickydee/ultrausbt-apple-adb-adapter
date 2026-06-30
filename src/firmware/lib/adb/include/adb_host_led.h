#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward USB HID keyboard LED output (bits 0–2) to the ADB keyboard via Listen R2. */
void adb_host_apply_usb_leds(uint8_t hid_leds);

#ifdef __cplusplus
}
#endif
