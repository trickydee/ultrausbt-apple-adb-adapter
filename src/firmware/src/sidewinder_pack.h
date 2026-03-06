/**
 * Pack Bluepad32 uni_gamepad_t into Microsoft Sidewinder 3D Pro ADB 56-bit packet.
 * See docs/ms-sidewinder-adb-packet.md and docs/adb-joystick-bluepad32-mapping.md.
 */

#ifndef SIDEWINDER_PACK_H
#define SIDEWINDER_PACK_H

#include <stdint.h>

#if ENABLE_BLUEPAD32

#define SIDEWINDER_PACKET_SIZE 7

#ifdef __cplusplus
extern "C" {
#endif

/** Pack gamepad state into 7-byte Sidewinder ADB packet. buffer must be SIDEWINDER_PACKET_SIZE bytes. */
void sidewinder_pack_from_gamepad(const void* gamepad, uint8_t* buffer);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_BLUEPAD32 */
#endif /* SIDEWINDER_PACK_H */
