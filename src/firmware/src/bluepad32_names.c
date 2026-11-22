/* Adapted from ultrausbt-amiga Bluepad32 name metadata.
 * Copyright (c) 2026 ultrausbt — SPDX-License-Identifier: EPL-2.0
 * Only display metadata; reconnect admission remains in shared Bluepad32.
 */
#include <btstack.h>
#include <ble/le_device_db.h>
#include <btstack_tlv.h>
#include <stdio.h>
#include <string.h>
#include <uni_log.h>
#include "bluepad32_names.h"

bool bluepad32_name_is_generic(const char *name) {
    return !name || !name[0] || strcmp(name, "BLE mouse") == 0 ||
        strcmp(name, "BLE keyboard") == 0 || strcmp(name, "BLE joystick") == 0 ||
        strcmp(name, "BLE gamepad") == 0;
}

// Display metadata in the existing BTstack flash store, keyed by bond slot.
// Verify the full identity on reads: a recycled bond slot must not inherit a name.
// UBN tags are private to ultrausbt; they contain no authentication material.
#define BLE_NAME_TAG_BASE 0x55424e00u
struct ble_name_record {
    uint8_t version, address_type;
    bd_addr_t address;
    char name[32];
};

static int ble_name_identity(uni_hid_device_t *device, struct ble_name_record *record) {
    if (device->conn.protocol != UNI_BT_CONN_PROTOCOL_BLE) return -1;
    int index = sm_le_device_index(device->conn.handle);
    if (index < 0 || index >= le_device_db_max_count() || index > 255) return -1;
    int type;
    le_device_db_info(index, &type, record->address, NULL);
    if (type != BD_ADDR_TYPE_LE_PUBLIC && type != BD_ADDR_TYPE_LE_RANDOM) return -1;
    record->version = 1;
    record->address_type = (uint8_t)type;
    return index;
}

void ultrausbt_restore_ble_name(uni_hid_device_t *device) {
    if (!bluepad32_name_is_generic(device->name)) return;
    struct ble_name_record identity = {0}, saved = {0};
    int index = ble_name_identity(device, &identity);
    if (index < 0) return;
    const btstack_tlv_t *tlv;
    void *context;
    btstack_tlv_get_instance(&tlv, &context);
    if (!tlv) return;
    int size = tlv->get_tag(context, BLE_NAME_TAG_BASE | (uint32_t)index,
                            (uint8_t *)&saved, sizeof(saved));
    if (size != sizeof(saved) || saved.version != identity.version ||
        saved.address_type != identity.address_type ||
        memcmp(saved.address, identity.address, sizeof(saved.address)) != 0 ||
        !saved.name[0] || !memchr(saved.name, 0, sizeof(saved.name))) return;
    uni_hid_device_set_name(device, saved.name);
}

void ultrausbt_save_ble_name(uni_hid_device_t *device) {
    // Never replace a useful saved name with the generic classification fallback.
    if (bluepad32_name_is_generic(device->name)) return;
    struct ble_name_record record = {0}, saved = {0};
    int index = ble_name_identity(device, &record);
    if (index < 0) return;
    snprintf(record.name, sizeof(record.name), "%.*s", (int)sizeof(record.name) - 1, device->name);
    const btstack_tlv_t *tlv;
    void *context;
    btstack_tlv_get_instance(&tlv, &context);
    if (!tlv) return;
    uint32_t tag = BLE_NAME_TAG_BASE | (uint32_t)index;
    int size = tlv->get_tag(context, tag, (uint8_t *)&saved, sizeof(saved));
    if (size == sizeof(saved) && memcmp(&saved, &record, sizeof(record)) == 0) return;
    if (tlv->store_tag(context, tag, (const uint8_t *)&record, sizeof(record)))
        loge("[BLE-RECONNECT] Could not persist display name\n");
}

void ultrausbt_clear_ble_names(void) {
    const btstack_tlv_t *tlv;
    void *context;
    btstack_tlv_get_instance(&tlv, &context);
    if (!tlv) return;
    for (int i = 0; i < le_device_db_max_count() && i <= 255; i++)
        tlv->delete_tag(context, BLE_NAME_TAG_BASE | (uint32_t)i);
}
