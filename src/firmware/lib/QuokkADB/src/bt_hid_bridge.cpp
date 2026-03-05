//----------------------------------------------------------------------------
//  HIDHopper ADB - Bluetooth HID bridge
//  Feeds Bluepad32 keyboard/mouse data into the same parsers as USB HID.
//  Do not include uni.h here to avoid HID type conflicts with TinyUSB.
//----------------------------------------------------------------------------

#include "platformkbdparser.h"
#include "platformmouseparser.h"
#include "adbkbdparser.h"
#include "adbmouseparser.h"
#include "tusb.h"

#if ENABLE_BLUEPAD32

#include "bluepad32_platform.h"

// Opaque data from Bluepad32 (must match uni_keyboard_t / uni_mouse_t layout)
struct bt_kbd_data_t {
    uint8_t modifiers;
    uint8_t pressed_keys[10];
    uint8_t reserved[16];
};

struct bt_mouse_data_t {
    int32_t delta_x;
    int32_t delta_y;
    uint16_t buttons;
    int8_t scroll_wheel;
    uint8_t misc_buttons;
};

// Fake dev_addr for BT so we don't collide with USB
static constexpr uint8_t BT_KBD_DEV_ADDR = 0x80;
static constexpr uint8_t BT_KBD_INSTANCE = 0;

extern ADBKbdRptParser KeyboardPrs;
extern ADBMouseRptParser MousePrs;

static void process_bluepad32_keyboard(void) {
    bt_kbd_data_t kbd;
    if (!bluepad32_get_keyboard(0, &kbd)) return;

    hid_keyboard_report_t report = {};
    report.modifier = kbd.modifiers;
    report.reserved = 0;
    for (int i = 0; i < 6; i++) {
        report.keycode[i] = kbd.pressed_keys[i];
    }

    KeyboardPrs.Parse(BT_KBD_DEV_ADDR, BT_KBD_INSTANCE, &report);
}

static void process_bluepad32_mouse(void) {
    bt_mouse_data_t mouse;
    if (!bluepad32_get_mouse(0, &mouse)) return;

    // Clamp deltas to int8_t range for HID boot report
    int8_t dx = (mouse.delta_x > 127) ? 127 : (mouse.delta_x < -128) ? -128 : (int8_t)mouse.delta_x;
    int8_t dy = (mouse.delta_y > 127) ? 127 : (mouse.delta_y < -128) ? -128 : (int8_t)mouse.delta_y;

    hid_mouse_report_t report = {};
    report.buttons = (uint8_t)(mouse.buttons & 0xFFu);  // left, right, middle
    report.x = dx;
    report.y = dy;
    report.wheel = mouse.scroll_wheel;
    report.pan = 0;

    MousePrs.Parse(&report);
}

void process_bluepad32_devices(void) {
    process_bluepad32_keyboard();
    process_bluepad32_mouse();
}

#else // !ENABLE_BLUEPAD32

void process_bluepad32_devices(void) {
    (void)0;
}

#endif
