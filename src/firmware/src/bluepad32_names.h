#pragma once
#include <stdbool.h>
#include <uni_hid_device.h>

bool bluepad32_name_is_generic(const char *name);
void ultrausbt_restore_ble_name(uni_hid_device_t *device);
void ultrausbt_save_ble_name(uni_hid_device_t *device);
void ultrausbt_clear_ble_names(void);
