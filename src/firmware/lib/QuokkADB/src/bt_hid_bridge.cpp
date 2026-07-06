//----------------------------------------------------------------------------
//  BT-USB-ADB-Adapter - Bluetooth HID bridge
//  Feeds Bluepad32 keyboard/mouse/gamepad data into the same parsers as USB HID.
//  Do not include uni.h here to avoid HID type conflicts with TinyUSB.
//----------------------------------------------------------------------------

#include "platformkbdparser.h"
#include "platformmouseparser.h"
#include "adbkbdparser.h"
#include "adbmouseparser.h"
#include "tusb.h"

#if ENABLE_BLUEPAD32

#include "bluepad32_platform.h"
#include "bluepad32_api.h"
#include "bt_host_coop.h"
#include "controller/uni_gamepad.h"

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

// Left stick → mouse: deadzone then scale per gamepad report (replace, not accumulate).
// See docs/gamepad-support.md § "Tuning: gamepad stick → mouse".
static constexpr int GP_MOUSE_DEADZONE = 64;
static constexpr int GP_MOUSE_MAX_DELTA = 40;

// Fake dev_addr for BT so we don't collide with USB
static constexpr uint8_t BT_KBD_DEV_ADDR = 0x80;
static constexpr uint8_t BT_KBD_INSTANCE = 0;

extern ADBKbdRptParser KeyboardPrs;
extern ADBMouseRptParser MousePrs;

static int append_unique_key(uint8_t* keys, int n, uint8_t k) {
    if (k == 0) return n;
    for (int i = 0; i < n; i++) {
        if (keys[i] == k) return n;
    }
    if (n < 6) keys[n++] = k;
    return n;
}

/** D-pad and buttons only — left stick moves the pointer via mouse deltas, not arrow keys. */
static int fill_keys_from_gamepad(const uni_gamepad_t* gp, uint8_t keys[6]) {
    int n = 0;
    if (gp->dpad & DPAD_UP) n = append_unique_key(keys, n, HID_KEY_ARROW_UP);
    if (gp->dpad & DPAD_DOWN) n = append_unique_key(keys, n, HID_KEY_ARROW_DOWN);
    if (gp->dpad & DPAD_LEFT) n = append_unique_key(keys, n, HID_KEY_ARROW_LEFT);
    if (gp->dpad & DPAD_RIGHT) n = append_unique_key(keys, n, HID_KEY_ARROW_RIGHT);
    if (gp->buttons & BUTTON_A) n = append_unique_key(keys, n, HID_KEY_SPACE);
    if (gp->buttons & BUTTON_B) n = append_unique_key(keys, n, HID_KEY_ESCAPE);
    if (gp->buttons & BUTTON_X) n = append_unique_key(keys, n, HID_KEY_Z);
    if (gp->buttons & BUTTON_Y) n = append_unique_key(keys, n, HID_KEY_X);
    if (gp->buttons & BUTTON_SHOULDER_R) n = append_unique_key(keys, n, HID_KEY_E);
    if (gp->buttons & BUTTON_TRIGGER_L) n = append_unique_key(keys, n, HID_KEY_1);
    if (gp->buttons & BUTTON_THUMB_L) n = append_unique_key(keys, n, HID_KEY_COMMA);
    if (gp->buttons & BUTTON_THUMB_R) n = append_unique_key(keys, n, HID_KEY_PERIOD);
    if (gp->misc_buttons & MISC_BUTTON_START) n = append_unique_key(keys, n, HID_KEY_ENTER);
    if (gp->misc_buttons & MISC_BUTTON_SELECT) n = append_unique_key(keys, n, HID_KEY_TAB);
    if (gp->misc_buttons & MISC_BUTTON_SYSTEM) n = append_unique_key(keys, n, HID_KEY_F1);
    if (gp->misc_buttons & MISC_BUTTON_CAPTURE) n = append_unique_key(keys, n, HID_KEY_F12);
    return n;
}

static void merge_keyboard_reports(const hid_keyboard_report_t* a, const hid_keyboard_report_t* b,
                                   hid_keyboard_report_t* merged) {
    merged->modifier = (uint8_t)(a->modifier | b->modifier);
    merged->reserved = 0;
    for (int i = 0; i < 6; i++) merged->keycode[i] = 0;
    int n = 0;
    for (int i = 0; i < 6 && a->keycode[i]; i++) n = append_unique_key(merged->keycode, n, a->keycode[i]);
    for (int i = 0; i < 6 && b->keycode[i]; i++) n = append_unique_key(merged->keycode, n, b->keycode[i]);
    for (int i = n; i < 6; i++) merged->keycode[i] = 0;
}

static bool keyboard_report_has_keys(const hid_keyboard_report_t* report) {
    if (report->modifier != 0) return true;
    for (int i = 0; i < 6; i++) {
        if (report->keycode[i] != 0) return true;
    }
    return false;
}

static bool peek_keyboard_reports_merged(hid_keyboard_report_t* out) {
    hid_keyboard_report_t merged = {};
    bool any = false;
    for (int i = 0; i < BLUEPAD32_MAX_BT_KEYBOARDS; i++) {
        bt_kbd_data_t kbd;
        if (!bluepad32_peek_keyboard(i, &kbd)) continue;
        hid_keyboard_report_t kr = {};
        kr.modifier = kbd.modifiers;
        kr.reserved = 0;
        for (int j = 0; j < 6; j++) {
            kr.keycode[j] = kbd.pressed_keys[j];
        }
        if (!any) {
            merged = kr;
            any = true;
        } else {
            hid_keyboard_report_t tmp;
            merge_keyboard_reports(&merged, &kr, &tmp);
            merged = tmp;
        }
    }
    if (any) *out = merged;
    return any;
}

/** Gamepad keys share the BT keyboard parser — omit during gamepad pairing or when a BT keyboard is not ready yet. */
static bool include_gamepad_keys_in_keyboard_report(void) {
    if (bluepad32_get_gamepad_count() == 0) return false;
    if (bluepad32_get_keyboard_count() == 0) return true;
    return core1_get_bt_pause_depth() == 0;
}

static bool build_merged_bt_keyboard_report(hid_keyboard_report_t* out) {
    hid_keyboard_report_t kb = {};
    const bool have_kb = peek_keyboard_reports_merged(&kb);

    hid_keyboard_report_t gp_keys = {};
    bool have_gp_keys = false;
    if (include_gamepad_keys_in_keyboard_report()) {
        uni_gamepad_t gp = {};
        if (bluepad32_peek_gamepad(0, &gp)) {
            gp_keys.modifier = 0;
            gp_keys.reserved = 0;
            fill_keys_from_gamepad(&gp, gp_keys.keycode);
            have_gp_keys = keyboard_report_has_keys(&gp_keys);
        }
    }

    if (have_kb && have_gp_keys) {
        merge_keyboard_reports(&kb, &gp_keys, out);
        return true;
    }
    if (have_kb) {
        *out = kb;
        return true;
    }
    if (have_gp_keys) {
        *out = gp_keys;
        return true;
    }
    return false;
}

static int8_t clamp_i32_to_i8_mouse(int32_t v) {
    if (v > 127) return 127;
    if (v < -128) return -128;
    return (int8_t)v;
}

/** Map virtual axis (~±512) to boot HID mouse delta; sign matches Bluepad32 (y negative = stick up). */
static int8_t scale_left_stick_to_mouse_delta(int32_t axis) {
    if (axis > -GP_MOUSE_DEADZONE && axis < GP_MOUSE_DEADZONE) return 0;
    int32_t adj = axis > 0 ? axis - GP_MOUSE_DEADZONE : axis + GP_MOUSE_DEADZONE;
    const int32_t denom = 512 - GP_MOUSE_DEADZONE;
    int32_t scaled = (adj * GP_MOUSE_MAX_DELTA) / denom;
    if (scaled > GP_MOUSE_MAX_DELTA) scaled = GP_MOUSE_MAX_DELTA;
    if (scaled < -GP_MOUSE_MAX_DELTA) scaled = -GP_MOUSE_MAX_DELTA;
    return (int8_t)scaled;
}

/** L1 → left click, R2 (right trigger) → right click (Bluepad32 virtual mask names). */
static uint8_t gamepad_mouse_button_mask(const uni_gamepad_t* gp) {
    uint8_t b = 0;
    if (gp->buttons & BUTTON_SHOULDER_L) b = (uint8_t)(b | MOUSE_BUTTON_LEFT);
    if (gp->buttons & BUTTON_TRIGGER_R) b = (uint8_t)(b | MOUSE_BUTTON_RIGHT);
    return b;
}

static void process_bluepad32_keyboard_and_gamepad(bool kb_new, bool gp_new) {
    if (!kb_new && !gp_new) {
        return;
    }

    hid_keyboard_report_t report = {};
    if (!build_merged_bt_keyboard_report(&report)) {
        return;
    }

    KeyboardPrs.Parse(BT_KBD_DEV_ADDR, BT_KBD_INSTANCE, &report);
}

/** Last-known BT mouse button mask (movement-only BLE reports omit buttons). */
static uint8_t peek_merged_bt_buttons(void) {
    bt_mouse_data_t mouse;
    uint8_t merged = 0;
    for (int i = 0; i < BLUEPAD32_MAX_BT_MICE; i++) {
        if (!bluepad32_peek_mouse(i, &mouse)) continue;
        merged = (uint8_t)(merged | (mouse.buttons & 0xFFu));
    }
    return merged;
}

static void reset_gamepad_mouse_latch_state(uint8_t* prev_gp_mouse_btn, uint8_t* prev_buttons,
                                            uint8_t* s_bt_buttons_latched, uint8_t* s_gp_buttons_latched,
                                            bool* gp_stick_was_moving) {
    *prev_gp_mouse_btn = 0;
    *prev_buttons = 0;
    *s_bt_buttons_latched = 0;
    *s_gp_buttons_latched = 0;
    *gp_stick_was_moving = false;
    MousePrs.ResetMouseMovement();
}

static void process_bluepad32_mouse_merged(const uni_gamepad_t* gp, bool gp_new) {
    static uint8_t prev_gp_mouse_btn = 0;
    static uint8_t prev_buttons = 0;
    static uint8_t s_bt_buttons_latched = 0;
    static uint8_t s_gp_buttons_latched = 0;
    static bool gp_stick_was_moving = false;
    static int prev_gp_count = 0;

    const int gp_count = bluepad32_get_gamepad_count();
    if (prev_gp_count > 0 && gp_count == 0) {
        reset_gamepad_mouse_latch_state(&prev_gp_mouse_btn, &prev_buttons, &s_bt_buttons_latched,
                                        &s_gp_buttons_latched, &gp_stick_was_moving);
    }
    prev_gp_count = gp_count;

    int16_t acc_dx = 0;
    int16_t acc_dy = 0;
    uint8_t buttons = peek_merged_bt_buttons();
    int8_t wheel = 0;

    bt_mouse_data_t mouse;
    for (int i = 0; i < BLUEPAD32_MAX_BT_MICE; i++) {
        while (bluepad32_get_mouse(i, &mouse)) {
            if (mouse.delta_x != 0 || mouse.delta_y != 0) {
                acc_dx += clamp_i32_to_i8_mouse(mouse.delta_x);
                acc_dy += clamp_i32_to_i8_mouse(mouse.delta_y);
            }
            uint8_t mb = (uint8_t)(mouse.buttons & 0xFFu);
            if (mb != 0) {
                buttons = (uint8_t)(buttons | mb);
            }
            if (mouse.scroll_wheel != 0) {
                wheel = mouse.scroll_wheel;
            }
        }
    }

    bool bt_motion = (acc_dx != 0 || acc_dy != 0);
    if (buttons != 0) {
        s_bt_buttons_latched = buttons;
    } else if (peek_merged_bt_buttons() == 0 && !bt_motion && s_gp_buttons_latched == 0) {
        s_bt_buttons_latched = 0;
    }

    uint8_t gp_mouse_btn = 0;
    int8_t gx = 0;
    int8_t gy = 0;
    if (gp_new && gp != nullptr) {
        gx = scale_left_stick_to_mouse_delta(gp->axis_x);
        gy = scale_left_stick_to_mouse_delta(gp->axis_y);
        gp_mouse_btn = gamepad_mouse_button_mask(gp);
        if (gp_mouse_btn != 0) {
            s_gp_buttons_latched = gp_mouse_btn;
        } else {
            s_gp_buttons_latched = 0;
        }
        buttons = (uint8_t)(buttons | gp_mouse_btn);
    }

    buttons = (uint8_t)(buttons | s_bt_buttons_latched | s_gp_buttons_latched);

    bool gp_motion = (gx != 0 || gy != 0);

    /* Only clear stick-sourced movement when the stick returns to centre. */
    if (gp_new && gp_motion) {
        gp_stick_was_moving = true;
    } else if (gp_new && !gp_motion && gp_stick_was_moving) {
        MousePrs.ResetMouseMovement();
        gp_stick_was_moving = false;
    }

    bool have_movement = bt_motion || gp_motion;
    bool gp_btn_edge = gp_new && (gp_mouse_btn != prev_gp_mouse_btn);
    bool btn_change = buttons != prev_buttons;
    bool gp_btn_held = (s_gp_buttons_latched != 0);
    bool emit = have_movement || (wheel != 0) || btn_change || gp_btn_edge
                || (gp_new && gp_btn_held);
    if (!emit) {
        return;
    }

    hid_mouse_report_t report = {};
    report.buttons = buttons;
    report.x = clamp_i32_to_i8_mouse((int32_t)acc_dx + gx);
    report.y = clamp_i32_to_i8_mouse((int32_t)acc_dy + gy);
    report.wheel = wheel;
    report.pan = 0;
    /* Replace pending delta only when the stick moved; USB/BT still accumulate. */
    MousePrs.Parse(&report, gp_motion);

    prev_buttons = buttons;
    if (gp_new) prev_gp_mouse_btn = gp_mouse_btn;
}

void process_bluepad32_devices(void) {
    bool kb_new = false;
    for (int i = 0; i < BLUEPAD32_MAX_BT_KEYBOARDS; i++) {
        bt_kbd_data_t kbd;
        if (bluepad32_get_keyboard(i, &kbd)) {
            kb_new = true;
        }
    }

    uni_gamepad_t gp = {};
    const bool gp_new = bluepad32_get_gamepad(0, &gp);

    process_bluepad32_keyboard_and_gamepad(kb_new, gp_new);
    process_bluepad32_mouse_merged(gp_new ? &gp : nullptr, gp_new);
}

#else  // !ENABLE_BLUEPAD32

void process_bluepad32_devices(void) {
    (void)0;
}

#endif
