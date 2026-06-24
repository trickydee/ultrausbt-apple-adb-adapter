/**
 * ADB passthrough hub — dual-port / daisy-chain address management.
 *
 * HIDHopper-style hardware connects two ADB sockets to one open-collector bus.
 * Hub mode does not hard-code addresses; it enables host-driven ADB enumeration
 * (Talk/Listen register 3, collision detection) with a bias to relocate the
 * USB mouse off 0x03 after a register-3 collision so chained trackballs can
 * keep the default pointing-device address.
 */

#ifndef ADB_HUB_H
#define ADB_HUB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Indices in QuokkADBSettings::reserved_bytes[] */
#define ADB_SETTINGS_IDX_HUB_MODE 0
#define ADB_SETTINGS_IDX_KBD_ADDR 1  /* reserved — host assigns addresses */
#define ADB_SETTINGS_IDX_MOUSE_ADDR 2

void adb_hub_configure(bool hub_mode);
void adb_hub_restore_addresses(void);
bool adb_hub_is_enabled(void);
uint8_t adb_hub_effective_kbd_addr(void);
uint8_t adb_hub_effective_mouse_addr(void);
bool adb_hub_is_local_address(uint8_t addr);

/** Address field (bits 11–8) for Talk register 3 before host Listen 0xFE. */
uint8_t adb_hub_propose_reg3_address(uint8_t current_addr, bool is_mouse);

/** Host confirmed address via Listen register 3 / handler 0xFE. */
void adb_hub_on_host_address_assigned(bool is_mouse);

bool adb_hub_is_host_assigned(bool is_mouse);
void adb_hub_clear_host_assigned(void);
void adb_hub_note_reg3_collision(bool is_mouse);

#ifdef __cplusplus
}
#endif

#endif /* ADB_HUB_H */
