/**
 * TinyUSB HID device (keyboard + mouse) for ADB host mode.
 */
#include "usb_hid_device.h"
#include "adb_host_led.h"
#include "adb_to_usb.h"
#include "tusb.h"
#include <string.h>

enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE = 2,
};

static uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
};

static uint8_t s_kbd_mod;
static uint8_t s_kbd_keys[6];
static uint8_t s_mouse_buttons;
static int8_t s_mouse_x;
static int8_t s_mouse_y;
static int8_t s_mouse_wheel;
static bool s_kbd_dirty;
static bool s_mouse_dirty;
static uint8_t s_usb_leds;
static uint8_t s_caps_pulse_phase;

#define USB_VID 0x2E8A
#define USB_PID 0xADB0

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL = 1 };
#define EPNUM_HID 0x81
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE, 10),
};

static char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "ultramegausb",
    "ADB Host Bridge",
    "0001",
};

void usb_hid_send_keyboard(uint8_t modifier, const uint8_t keycodes[6])
{
    s_kbd_mod = modifier;
    memcpy(s_kbd_keys, keycodes, 6);
    s_kbd_dirty = true;
}

void usb_hid_send_mouse(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
{
    s_mouse_buttons = buttons;
    s_mouse_x = dx;
    s_mouse_y = dy;
    s_mouse_wheel = wheel;
    s_mouse_dirty = true;
}

void usb_hid_set_led_state(uint8_t hid_leds)
{
    s_usb_leds = hid_leds;
}

void usb_hid_pulse_caps_lock(void)
{
    if (s_caps_pulse_phase == 0) {
        s_caps_pulse_phase = 1;
    }
}

void usb_hid_device_task(void)
{
    if (!tud_mounted()) {
        return;
    }
    if (s_caps_pulse_phase != 0 && tud_hid_ready()) {
        if (s_caps_pulse_phase == 1) {
            uint8_t keys[6] = {HID_KEY_CAPS_LOCK};
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keys);
            s_caps_pulse_phase = 2;
        } else {
            uint8_t keys[6] = {0};
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keys);
            s_caps_pulse_phase = 0;
        }
        return;
    }
    if (s_kbd_dirty && tud_hid_ready()) {
        s_kbd_dirty = false;
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, s_kbd_mod, s_kbd_keys);
    }
    if (s_mouse_dirty && tud_hid_ready()) {
        s_mouse_dirty = false;
        tud_hid_mouse_report(REPORT_ID_MOUSE, s_mouse_buttons, s_mouse_x, s_mouse_y, s_mouse_wheel, 0);
    }
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return desc_hid_report;
}

uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t desc_str[32];
    if (index == 0) {
        memcpy(&desc_str[1], string_desc_arr[0] + 1, 1);
        desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 1);
        return desc_str;
    }
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
        return NULL;
    }
    const char *str = string_desc_arr[index];
    uint8_t len = (uint8_t)strlen(str);
    if (len > 31) {
        len = 31;
    }
    for (uint8_t i = 0; i < len; i++) {
        desc_str[1 + i] = str[i];
    }
    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * len + 2);
    return desc_str;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    if (report_type == HID_REPORT_TYPE_OUTPUT && buffer && reqlen >= 1) {
        buffer[0] = s_usb_leds;
        return 1;
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    if (report_type != HID_REPORT_TYPE_OUTPUT || !buffer || bufsize < 1) {
        return;
    }
    s_usb_leds = buffer[0];
    adb_to_usb_note_host_leds(buffer[0]);
    adb_host_apply_usb_leds(buffer[0]);
}
