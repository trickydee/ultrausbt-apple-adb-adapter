/**
 * Bluetooth pairing sync: flash-safe core init and core1 pause during pairing.
 * Reduces hangs when pairing keyboards, mice, and gamepads (see amigahid-pico).
 */

#ifndef BT_PAIRING_SYNC_H
#define BT_PAIRING_SYNC_H

#if ENABLE_BLUEPAD32

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Call once from Core 1 entry (before main loop). Enables flash coordination so
 * Core 0 (btstack) can safely write TLV to flash during pairing. */
void bt_pairing_sync_core1_init(void);

/** Returns true when Core 1 should pause (e.g. during GATT discovery). Core 1 loop checks this. */
bool bt_pairing_sync_is_core1_paused(void);

/** Call from platform when starting connection/paring (device_connected, or device_discovered for HID). */
void bt_pairing_sync_pause_core1(void);

/** Call from platform after device_ready (with a short sleep before this), or on device_disconnected. */
void bt_pairing_sync_resume_core1(void);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_BLUEPAD32 */
#endif /* BT_PAIRING_SYNC_H */
