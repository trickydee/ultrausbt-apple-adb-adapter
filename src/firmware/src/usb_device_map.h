/**
 * USB device names and counts for the Map Devices OLED screen.
 * Core 1 (USB host) updates state; Core 0 reads for display (no I2C from Core 1).
 */

#ifndef USB_DEVICE_MAP_H
#define USB_DEVICE_MAP_H

#include <stdbool.h>
#include <stdint.h>

#define USB_MAP_NAME_LEN 24

void usb_map_register_gamepad(uint8_t dev_addr, const char* name);
void usb_map_unregister_gamepad(uint8_t dev_addr);
bool usb_map_gamepad_registered(uint8_t dev_addr);
const char* usb_map_get_gamepad(int slot);

void usb_map_set_keyboard(const char* name);
void usb_map_clear_keyboard(void);
const char* usb_map_get_keyboard(void);

void usb_map_set_mouse(const char* name);
void usb_map_clear_mouse(void);
const char* usb_map_get_mouse(void);

void usb_map_on_keyboard_mount(void);
void usb_map_on_keyboard_umount(void);

void usb_map_get_counts(uint8_t* kb, uint8_t* mouse, uint8_t* joy);

#endif /* USB_DEVICE_MAP_H */
