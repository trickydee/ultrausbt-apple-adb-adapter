/**
 * USB device names and counts for the Map Devices OLED screen.
 */

#include "usb_device_map.h"
#include <string.h>

typedef struct {
    uint8_t dev_addr;
    char name[USB_MAP_NAME_LEN];
    bool used;
} usb_joy_slot_t;

static usb_joy_slot_t joy_slots[2];
static char keyboard_name[USB_MAP_NAME_LEN];
static bool keyboard_present;
static char mouse_name[USB_MAP_NAME_LEN];
static bool mouse_present;
static volatile uint8_t usb_kb_count;
static volatile uint8_t usb_mouse_count;

static void copy_name(char* dst, size_t len, const char* src)
{
    if (!src || !src[0]) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
}

static void compact_joy_slots(void)
{
    if (!joy_slots[0].used && joy_slots[1].used) {
        joy_slots[0] = joy_slots[1];
        joy_slots[1].used = false;
        joy_slots[1].dev_addr = 0;
        joy_slots[1].name[0] = '\0';
    }
}

static uint8_t joy_slot_count(void)
{
    uint8_t n = 0;
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used) {
            n++;
        }
    }
    return n;
}

void usb_map_register_gamepad(uint8_t dev_addr, const char* name)
{
    if (!dev_addr) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used && joy_slots[i].dev_addr == dev_addr) {
            copy_name(joy_slots[i].name, sizeof(joy_slots[i].name), name);
            return;
        }
    }
    for (int i = 0; i < 2; i++) {
        if (!joy_slots[i].used) {
            joy_slots[i].used = true;
            joy_slots[i].dev_addr = dev_addr;
            copy_name(joy_slots[i].name, sizeof(joy_slots[i].name), name);
            return;
        }
    }
}

void usb_map_unregister_gamepad(uint8_t dev_addr)
{
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used && joy_slots[i].dev_addr == dev_addr) {
            joy_slots[i].used = false;
            joy_slots[i].dev_addr = 0;
            joy_slots[i].name[0] = '\0';
            compact_joy_slots();
            return;
        }
    }
}

bool usb_map_gamepad_registered(uint8_t dev_addr)
{
    if (!dev_addr) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (joy_slots[i].used && joy_slots[i].dev_addr == dev_addr) {
            return true;
        }
    }
    return false;
}

const char* usb_map_get_gamepad(int slot)
{
    if (slot < 0 || slot > 1 || !joy_slots[slot].used) {
        return NULL;
    }
    return joy_slots[slot].name;
}

void usb_map_set_keyboard(const char* name)
{
    keyboard_present = true;
    copy_name(keyboard_name, sizeof(keyboard_name), name);
}

void usb_map_clear_keyboard(void)
{
    keyboard_present = false;
    keyboard_name[0] = '\0';
}

const char* usb_map_get_keyboard(void)
{
    return keyboard_present ? keyboard_name : NULL;
}

void usb_map_set_mouse(const char* name)
{
    mouse_present = true;
    usb_mouse_count = 1;
    copy_name(mouse_name, sizeof(mouse_name), name);
}

void usb_map_clear_mouse(void)
{
    mouse_present = false;
    usb_mouse_count = 0;
    mouse_name[0] = '\0';
}

const char* usb_map_get_mouse(void)
{
    return mouse_present ? mouse_name : NULL;
}

void usb_map_on_keyboard_mount(void)
{
    if (usb_kb_count < 255) {
        usb_kb_count++;
    }
    if (!keyboard_present) {
        usb_map_set_keyboard("USB Keyboard");
    }
}

void usb_map_on_keyboard_umount(void)
{
    if (usb_kb_count > 0) {
        usb_kb_count--;
    }
    if (usb_kb_count == 0) {
        usb_map_clear_keyboard();
    }
}

void usb_map_get_counts(uint8_t* kb, uint8_t* mouse, uint8_t* joy)
{
    if (kb) {
        *kb = usb_kb_count;
    }
    if (mouse) {
        *mouse = usb_mouse_count;
    }
    if (joy) {
        *joy = joy_slot_count();
    }
}
