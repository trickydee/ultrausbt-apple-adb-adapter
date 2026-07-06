#include "adb_to_usb.h"
#include "adbregisters.h"
#include "usb_hid_keys.h"
#include "usb_hid_device.h"
#include "class/hid/hid.h"
#include <string.h>

extern uint8_t usb_keycode_to_adb_code(uint8_t usb_code);

static constexpr uint8_t kAdbCapsLock = 0x39;

static uint8_t s_adb_to_usb[128];
static uint8_t s_keys[6];
static uint8_t s_mod;
static uint8_t s_host_leds;
static bool s_map_ready = false;

static void build_reverse_map(void)
{
    for (unsigned i = 0; i < 128; i++) {
        s_adb_to_usb[i] = 0;
    }
    for (unsigned usb = 0; usb < 256; usb++) {
        uint8_t adb = usb_keycode_to_adb_code((uint8_t)usb);
        if (adb < 0x7F) {
            s_adb_to_usb[adb] = (uint8_t)usb;
        }
    }
}

extern "C" void adb_to_usb_init(void)
{
    if (!s_map_ready) {
        build_reverse_map();
        s_map_ready = true;
    }
    memset(s_keys, 0, sizeof(s_keys));
    s_mod = 0;
    s_host_leds = 0;
}

extern "C" void adb_to_usb_note_host_leds(uint8_t hid_leds)
{
    s_host_leds = hid_leds;
    usb_hid_set_led_state(hid_leds);
}

extern "C" void adb_to_usb_apply_reg2(uint16_t reg2)
{
    bool kbd_caps = !(reg2 & (1u << ADB_REG_2_FLAG_CAPS_LOCK_LED));
    bool pc_caps = (s_host_leds & 0x02u) != 0;

    if (kbd_caps != pc_caps) {
        usb_hid_pulse_caps_lock();
        s_host_leds ^= 0x02u;
    }
}

static uint8_t adb_code_to_modifier(uint8_t adb_code)
{
    switch (adb_code) {
    case 0x36:
        return KEYBOARD_MODIFIER_LEFTCTRL;
    case 0x37:
        return KEYBOARD_MODIFIER_LEFTGUI;
    case 0x38:
        return KEYBOARD_MODIFIER_LEFTSHIFT;
    case 0x3A:
        return KEYBOARD_MODIFIER_LEFTALT;
    case 0x7B:
        return KEYBOARD_MODIFIER_RIGHTSHIFT;
    case 0x7C:
        return KEYBOARD_MODIFIER_RIGHTALT;
    case 0x7D:
        return KEYBOARD_MODIFIER_RIGHTCTRL;
    default:
        return 0;
    }
}

static void set_modifier(uint8_t bit, bool key_up)
{
    if (key_up) {
        s_mod &= (uint8_t)~bit;
    } else {
        s_mod |= bit;
    }
}

static int8_t seven_to_eight_signed(uint8_t seven)
{
    int8_t v = (int8_t)(seven & 0x7F);
    if (seven & 0x40) {
        v = (int8_t)(v - 128);
    }
    return (int8_t)(v * 2);
}

static bool adb_keycode_valid(uint8_t adb_code)
{
    return adb_code != ADB_REG_0_NO_KEY && adb_code < 0x7F;
}

static void push_key(uint8_t adb_code, bool key_up)
{
    if (!adb_keycode_valid(adb_code)) {
        return;
    }
    // Locking caps on vintage ADB keyboards — latch/LED comes from register 2, not R0.
    if (adb_code == kAdbCapsLock) {
        return;
    }
    uint8_t mod_bit = adb_code_to_modifier(adb_code);
    if (mod_bit) {
        set_modifier(mod_bit, key_up);
        return;
    }
    uint8_t usb = s_adb_to_usb[adb_code];
    if (!usb) {
        return;
    }
    if (key_up) {
        for (int i = 0; i < 6; i++) {
            if (s_keys[i] == usb) {
                s_keys[i] = 0;
            }
        }
        return;
    }
    for (int i = 0; i < 6; i++) {
        if (s_keys[i] == usb) {
            return;
        }
    }
    for (int i = 0; i < 6; i++) {
        if (s_keys[i] == 0) {
            s_keys[i] = usb;
            return;
        }
    }
}

extern "C" void adb_to_usb_keyboard_reg0(uint16_t reg0)
{
    if (reg0 == 0) {
        memset(s_keys, 0, sizeof(s_keys));
        s_mod = 0;
        usb_hid_send_keyboard(s_mod, s_keys);
        return;
    }

    uint8_t key1 = (uint8_t)((reg0 >> ADB_REG_0_KEY_1_KEY_CODE) & 0x7F);
    uint8_t key2 = (uint8_t)((reg0 >> ADB_REG_0_KEY_2_KEY_CODE) & 0x7F);
    bool key1_up = (reg0 & (1u << ADB_REG_0_KEY_1_STATUS_BIT)) != 0;
    bool key2_up = (reg0 & (1u << ADB_REG_0_KEY_2_STATUS_BIT)) != 0;

    if (!adb_keycode_valid(key1) && !adb_keycode_valid(key2)) {
        memset(s_keys, 0, sizeof(s_keys));
        s_mod = 0;
        usb_hid_send_keyboard(s_mod, s_keys);
        return;
    }

    if (adb_keycode_valid(key1)) {
        push_key(key1, key1_up);
    }
    if (adb_keycode_valid(key2)) {
        push_key(key2, key2_up);
    }

    usb_hid_send_keyboard(s_mod, s_keys);
}

extern "C" void adb_to_usb_mouse_reg0(uint16_t reg0)
{
    uint8_t buttons = 0;
    if ((reg0 & (1u << 15)) == 0) {
        buttons |= 0x01;
    }
    if ((reg0 & (1u << 7)) == 0) {
        buttons |= 0x02;
    }

    int8_t dy = seven_to_eight_signed((uint8_t)((reg0 >> 8) & 0x7F));
    int8_t dx = seven_to_eight_signed((uint8_t)(reg0 & 0x7F));

    usb_hid_send_mouse(buttons, dx, dy, 0);
}
