/**
 * ADB host bus device status for UI (configured vs responding on Talk R0).
 */
#ifndef ADB_HOST_STATUS_H
#define ADB_HOST_STATUS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADB_HOST_STATUS_MAX 5

typedef struct {
    uint8_t addr;
    uint8_t is_keyboard;
    uint8_t working;
} adb_host_device_status_t;

typedef struct {
    uint8_t count;
    uint8_t kbd_configured;
    uint8_t kbd_working;
    uint8_t mouse_configured;
    uint8_t mouse_working;
    adb_host_device_status_t devices[ADB_HOST_STATUS_MAX];
} adb_host_status_t;

#ifdef __cplusplus
}
#endif

#endif /* ADB_HOST_STATUS_H */
