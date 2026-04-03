/**
 * Core 0 (Bluetooth / ADB) ↔ Core 1 (USB host) cooperation.
 * During heavy BT gamepad enumeration (e.g. Stadia), Core 1 stops calling tuh_task()
 * so flash/XIP coordination matches the pattern used in amigahid-pico (quad_mouse pause).
 */

#ifndef BT_HOST_COOP_H
#define BT_HOST_COOP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bt_host_coop_usb_host_set_paused(bool paused);
bool bt_host_coop_usb_host_is_paused(void);

#ifdef __cplusplus
}
#endif

#endif
