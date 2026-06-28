/**
 * ADB operating mode: device (USB/BT → vintage Mac) vs host (ADB accessories → PC USB).
 * Manual switch only — see docs/adb-host-mode.md.
 */
#ifndef ADB_MODE_H
#define ADB_MODE_H

#include <stdbool.h>
#include <stdint.h>

struct FlashSettings;

#ifdef __cplusplus
extern "C" {
#endif

#define ADB_SETTINGS_IDX_ADB_MODE 3

typedef enum {
    ADB_MODE_DEVICE = 0,
    ADB_MODE_HOST = 1,
} adb_operating_mode_t;

void adb_mode_init(struct FlashSettings *settings);
adb_operating_mode_t adb_mode_get(void);
bool adb_mode_is_host(void);
bool adb_mode_request(adb_operating_mode_t mode, bool persist);
void adb_mode_apply_pending(void);
bool adb_mode_host_active(void);
bool adb_mode_usb_is_device(void);
void adb_mode_usb_sync(void);

#ifdef __cplusplus
}
#endif

#endif /* ADB_MODE_H */
