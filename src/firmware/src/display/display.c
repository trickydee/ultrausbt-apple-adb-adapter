/**
 * Display interface for SSD1306 OLED – Apple ADB / BT-USB-ADB-Adapter
 * Aligned with ULTRAMEGAUSB_OLED_UI_SPEC (splash, devices, map devices).
 */

#include "display/display.h"
#include "display/display_config.h"
#include "ssd1306.h"
#include "usb_device_map.h"
#include "adb_hub.h"
#if ADB_HOST_MODE
#include "adb_mode.h"
#include "adb_host_status.h"
#endif
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/time.h>
#include <stdio.h>
#include <string.h>

#if ENABLE_BLUEPAD32
#include "bluepad32_init.h"
#include "bluepad32_platform.h"
#endif

#ifndef BT_USB_ADB_ADAPTER_VERSION_STRING
#define BT_USB_ADB_ADAPTER_VERSION_STRING "?.?.?"
#endif

#define PAIRING_CLEAR_HOLD_MS 5000

static ssd1306_t disp;

static volatile uint8_t usb_kb_count = 0;
static volatile uint8_t usb_mouse_count = 0;
static volatile uint8_t usb_gamepad_count = 0;
static volatile uint8_t bt_kb_count = 0;
static volatile uint8_t bt_mouse_count = 0;
static volatile uint8_t bt_gamepad_count = 0;

static uint8_t last_drawn_usb_kb = 0xFF;
static uint8_t last_drawn_usb_mouse = 0xFF;
static uint8_t last_drawn_usb_game = 0xFF;
static uint8_t last_drawn_bt_kb = 0xFF;
static uint8_t last_drawn_bt_mouse = 0xFF;
static uint8_t last_drawn_bt_game = 0xFF;

static int adb_connected = 0;
static uint8_t adb_kbd_id = 0;
static uint8_t adb_mouse_id = 0;
static uint8_t adb_game_id = 0;
static int adb_srq = 0;
static int adb_collision = 0;
static int last_drawn_adb_connected = -1;
static uint8_t last_drawn_adb_kbd = 0xFF;
static uint8_t last_drawn_adb_mouse = 0xFF;
static uint8_t last_drawn_adb_game = 0xFF;
static int last_drawn_adb_srq = -1;
static int last_drawn_adb_collision = -1;

static display_screen_t current_screen = DISPLAY_SCREEN_SPLASH;

#if ADB_HOST_MODE
static adb_operating_mode_t mode_ui_selection = ADB_MODE_DEVICE;
static adb_host_status_t adb_host_status;
static adb_host_status_t last_drawn_adb_host_status;
#endif

#define BUTTON_DEBOUNCE_COUNT 3
static uint8_t button_middle_debounce = 0;
static uint8_t button_left_debounce = 0;
static uint8_t button_right_debounce = 0;

#if ENABLE_BLUEPAD32
static bool pairing_clear_active = false;
static absolute_time_t pairing_clear_started;
static int pairing_clear_last_second = -1;
#endif

static void draw_map_row(int y, const char* label, const char* name)
{
    char buf[32];
    if (name && name[0]) {
        snprintf(buf, sizeof(buf), "%s:%.20s", label, name);
    } else {
        snprintf(buf, sizeof(buf), "%s: --", label);
    }
    ssd1306_draw_string(&disp, 0, y, 1, buf);
}

#if ENABLE_BLUEPAD32
static const char* map_name_bt_usb(char bt_type, int bt_idx, const char* (*usb_get)(void))
{
    const char* name = bluepad32_get_device_name(bt_type, bt_idx);
    if (name) {
        return name;
    }
    if (bt_idx == 0 && usb_get) {
        return usb_get();
    }
    return NULL;
}
#endif

void display_init(void)
{
    i2c_init(SSD1306_I2C, 400000);
    gpio_set_function(SSD1306_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_PIN_SDA);
    gpio_pull_up(SSD1306_PIN_SCL);

    gpio_init(DISPLAY_GPIO_BUTTON_LEFT);
    gpio_set_dir(DISPLAY_GPIO_BUTTON_LEFT, GPIO_IN);
    gpio_pull_up(DISPLAY_GPIO_BUTTON_LEFT);

    gpio_init(DISPLAY_GPIO_BUTTON_MIDDLE);
    gpio_set_dir(DISPLAY_GPIO_BUTTON_MIDDLE, GPIO_IN);
    gpio_pull_up(DISPLAY_GPIO_BUTTON_MIDDLE);

    gpio_init(DISPLAY_GPIO_BUTTON_RIGHT);
    gpio_set_dir(DISPLAY_GPIO_BUTTON_RIGHT, GPIO_IN);
    gpio_pull_up(DISPLAY_GPIO_BUTTON_RIGHT);

    if (ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_ADDR, SSD1306_I2C)) {
        display_show_splash();
    }
}

void display_set_adb_status(int connected, uint8_t kbd_id, uint8_t mouse_id, uint8_t game_id, int srq, int collision)
{
    adb_connected = connected ? 1 : 0;
    adb_kbd_id = kbd_id;
    adb_mouse_id = mouse_id;
    adb_game_id = game_id;
    adb_srq = srq ? 1 : 0;
    adb_collision = collision ? 1 : 0;
    if (current_screen != DISPLAY_SCREEN_SPLASH) {
        return;
    }
    if (last_drawn_adb_connected != adb_connected ||
        last_drawn_adb_kbd != adb_kbd_id ||
        last_drawn_adb_mouse != adb_mouse_id ||
        last_drawn_adb_game != adb_game_id) {
        last_drawn_adb_connected = adb_connected;
        last_drawn_adb_kbd = adb_kbd_id;
        last_drawn_adb_mouse = adb_mouse_id;
        last_drawn_adb_game = adb_game_id;
        last_drawn_adb_srq = adb_srq;
        last_drawn_adb_collision = adb_collision;
        display_show_splash();
    }
}

void display_show_splash(void)
{
    char line[24];

    ssd1306_clear(&disp);

    ssd1306_draw_string(&disp, 46, 0, 2, (char *)"ADB");
    ssd1306_draw_string(&disp, 4, 24, 1, (char *)"ultramegausb.com");

    snprintf(line, sizeof(line), "v%s", BT_USB_ADB_ADAPTER_VERSION_STRING);
    ssd1306_draw_string(&disp, 40, 40, 1, line);

#if ADB_HOST_MODE
    snprintf(line, sizeof(line), "Mode: %s", adb_mode_is_host() ? "ADB>USB" : "ADB>Mac");
    ssd1306_draw_string(&disp, 0, 48, 1, line);
#endif

#if ADB_HOST_MODE
    if (adb_mode_host_active()) {
        snprintf(line, sizeof(line), "Bus K%u/%u M%u/%u",
                 (unsigned)adb_host_status.kbd_working, (unsigned)adb_host_status.kbd_configured,
                 (unsigned)adb_host_status.mouse_working, (unsigned)adb_host_status.mouse_configured);
        ssd1306_draw_string(&disp, 0, 55, 1, line);
    } else if (!adb_connected) {
        ssd1306_draw_string(&disp, 0, 55, 1, (char *)"ADB: --");
    } else {
        snprintf(line, sizeof(line), "ADB: K%X M%X G%X%s%s",
                (unsigned)adb_kbd_id, (unsigned)adb_mouse_id, (unsigned)adb_game_id,
                adb_srq ? " S" : "", adb_collision ? "!" : "");
        ssd1306_draw_string(&disp, 0, 55, 1, line);
    }
#else
    if (!adb_connected) {
        ssd1306_draw_string(&disp, 0, 55, 1, (char *)"ADB: --");
    } else {
        snprintf(line, sizeof(line), "ADB: K%X M%X G%X%s%s",
                (unsigned)adb_kbd_id, (unsigned)adb_mouse_id, (unsigned)adb_game_id,
                adb_srq ? " S" : "", adb_collision ? "!" : "");
        ssd1306_draw_string(&disp, 0, 55, 1, line);
    }
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SPLASH;
    last_drawn_bt_kb = bt_kb_count;
    last_drawn_bt_mouse = bt_mouse_count;
    last_drawn_bt_game = bt_gamepad_count;
    last_drawn_adb_connected = adb_connected;
    last_drawn_adb_kbd = adb_kbd_id;
    last_drawn_adb_mouse = adb_mouse_id;
    last_drawn_adb_game = adb_game_id;
    last_drawn_adb_srq = adb_srq;
    last_drawn_adb_collision = adb_collision;
}

void display_show_devices(void)
{
    char buf[32];

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"Devices");

    snprintf(buf, sizeof(buf), "Keybd   U %d BT %d", (int)usb_kb_count, (int)bt_kb_count);
    ssd1306_draw_string(&disp, 0, 9, 1, buf);

    snprintf(buf, sizeof(buf), "Mouse   U %d BT %d", (int)usb_mouse_count, (int)bt_mouse_count);
    ssd1306_draw_string(&disp, 0, 18, 1, buf);

    snprintf(buf, sizeof(buf), "Game    U %d BT %d", (int)usb_gamepad_count, (int)bt_gamepad_count);
    ssd1306_draw_string(&disp, 0, 27, 1, buf);

#if ENABLE_BLUEPAD32
    if (bt_gamepad_count > 0) {
        ssd1306_draw_string(&disp, 0, 36, 1, (char *)"GP: Kbd+Mouse");
    }
#endif

    if (adb_hub_is_enabled()) {
        snprintf(buf, sizeof(buf), "Hub K%X M%X", (unsigned)adb_hub_effective_kbd_addr(),
                (unsigned)adb_hub_effective_mouse_addr());
        ssd1306_draw_string(&disp, 0, 55, 1, buf);
    }

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_DEVICES;
    last_drawn_usb_kb = usb_kb_count;
    last_drawn_usb_mouse = usb_mouse_count;
    last_drawn_usb_game = usb_gamepad_count;
    last_drawn_bt_kb = bt_kb_count;
    last_drawn_bt_mouse = bt_mouse_count;
    last_drawn_bt_game = bt_gamepad_count;
}

void display_show_map_devices(void)
{
    int row = 9;

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"Map Devices");

#if ENABLE_BLUEPAD32
    const char* g1 = bluepad32_get_device_name('G', 0);
    if (!g1) {
        g1 = usb_map_get_gamepad(0);
    }
    draw_map_row(row, "G1", g1);
    row += 9;

    draw_map_row(row, "K1", map_name_bt_usb('K', 0, usb_map_get_keyboard));
    row += 9;

    draw_map_row(row, "M1", map_name_bt_usb('M', 0, usb_map_get_mouse));
    row += 9;

    if (bt_kb_count > 1) {
        draw_map_row(row, "K2", bluepad32_get_device_name('K', 1));
        row += 9;
    }
    if (bt_mouse_count > 1) {
        draw_map_row(row, "M2", bluepad32_get_device_name('M', 1));
    }

    ssd1306_draw_string(&disp, 0, 55, 1, (char *)"L+R: clear pairings");
#else
    draw_map_row(9, "G1", usb_map_get_gamepad(0));
    draw_map_row(18, "K1", usb_map_get_keyboard());
    draw_map_row(27, "M1", usb_map_get_mouse());
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_MAP_DEVICES;
    last_drawn_bt_kb = bt_kb_count;
    last_drawn_bt_mouse = bt_mouse_count;
    last_drawn_bt_game = bt_gamepad_count;
}

#if ADB_HOST_MODE
static bool adb_host_status_changed(const adb_host_status_t *a, const adb_host_status_t *b)
{
    if (a->count != b->count ||
        a->kbd_configured != b->kbd_configured ||
        a->kbd_working != b->kbd_working ||
        a->mouse_configured != b->mouse_configured ||
        a->mouse_working != b->mouse_working) {
        return true;
    }
    for (uint8_t i = 0; i < a->count; i++) {
        if (a->devices[i].addr != b->devices[i].addr ||
            a->devices[i].is_keyboard != b->devices[i].is_keyboard ||
            a->devices[i].working != b->devices[i].working) {
            return true;
        }
    }
    return false;
}

void display_show_adb_bus(void)
{
    char buf[32];
    int row = 9;

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"ADB Bus");

    if (!adb_mode_host_active()) {
        ssd1306_draw_string(&disp, 0, 18, 1, (char *)"ADB>Mac mode");
        ssd1306_draw_string(&disp, 0, 30, 1, (char *)"Switch to ADB>USB");
        ssd1306_draw_string(&disp, 0, 55, 1, (char *)"to scan bus");
    } else if (adb_host_status.count == 0) {
        ssd1306_draw_string(&disp, 0, 18, 1, (char *)"Scanning...");
    } else {
        for (uint8_t i = 0; i < adb_host_status.count && row <= 45; i++) {
            const adb_host_device_status_t *dev = &adb_host_status.devices[i];
            snprintf(buf, sizeof(buf), "%s @%X %s",
                     dev->is_keyboard ? "Key" : "Mou",
                     (unsigned)dev->addr,
                     dev->working ? "OK" : "--");
            ssd1306_draw_string(&disp, 0, row, 1, buf);
            row += 9;
        }

        snprintf(buf, sizeof(buf), "K %u/%u  M %u/%u",
                 (unsigned)adb_host_status.kbd_working, (unsigned)adb_host_status.kbd_configured,
                 (unsigned)adb_host_status.mouse_working, (unsigned)adb_host_status.mouse_configured);
        ssd1306_draw_string(&disp, 0, 55, 1, buf);
    }

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_ADB_BUS;
    last_drawn_adb_host_status = adb_host_status;
}

void display_set_adb_host_status(const adb_host_status_t *status)
{
    if (!status) {
        return;
    }
    adb_host_status = *status;
    if (current_screen == DISPLAY_SCREEN_ADB_BUS &&
        adb_host_status_changed(status, &last_drawn_adb_host_status)) {
        display_show_adb_bus();
    } else if (current_screen == DISPLAY_SCREEN_SPLASH && adb_mode_host_active()) {
        display_show_splash();
    }
}

void display_show_mode(void)
{
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"ADB Mode");

    if (mode_ui_selection == ADB_MODE_DEVICE) {
        ssd1306_draw_string(&disp, 0, 16, 1, (char *)"> ADB > Mac");
        ssd1306_draw_string(&disp, 0, 28, 1, (char *)"  ADB > USB");
    } else {
        ssd1306_draw_string(&disp, 0, 16, 1, (char *)"  ADB > Mac");
        ssd1306_draw_string(&disp, 0, 28, 1, (char *)"> ADB > USB");
    }

    ssd1306_draw_string(&disp, 0, 44, 1, (char *)"USB kbd/mouse OFF");
    ssd1306_draw_string(&disp, 0, 55, 1, (char *)"Mid=apply L/R=sel");
    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_MODE;
}

static void mode_screen_apply(void)
{
    adb_mode_request(mode_ui_selection, true);
    adb_mode_apply_pending();
    display_show_splash();
}
#endif

void display_update_devices(void)
{
    if (current_screen == DISPLAY_SCREEN_DEVICES) {
        display_show_devices();
    }
    if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
        display_show_map_devices();
    }
#if ADB_HOST_MODE
    if (current_screen == DISPLAY_SCREEN_ADB_BUS) {
        display_show_adb_bus();
    }
#endif
}

void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    usb_kb_count = kb;
    usb_mouse_count = mouse;
    usb_gamepad_count = joy;
    if (last_drawn_usb_kb != kb || last_drawn_usb_mouse != mouse || last_drawn_usb_game != joy) {
        last_drawn_usb_kb = kb;
        last_drawn_usb_mouse = mouse;
        last_drawn_usb_game = joy;
        display_update_devices();
    }
}

void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    bt_kb_count = kb;
    bt_mouse_count = mouse;
    bt_gamepad_count = joy;
    if (last_drawn_bt_kb != kb || last_drawn_bt_mouse != mouse || last_drawn_bt_game != joy) {
        last_drawn_bt_kb = kb;
        last_drawn_bt_mouse = mouse;
        last_drawn_bt_game = joy;
        display_update_devices();
        if (current_screen == DISPLAY_SCREEN_SPLASH) {
            display_show_splash();
        }
    }
}

void display_show_controller_detected(const char *controller_name, const char *controller_model, uint32_t duration_ms)
{
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 25, 10, 2, (char *)controller_name);
    if (controller_model) {
        ssd1306_draw_string(&disp, 10, 35, 1, (char *)controller_model);
    }
    ssd1306_show(&disp);
    sleep_ms(duration_ms);
    display_show_splash();
}

#if ENABLE_BLUEPAD32
static void show_pairing_clear_overlay(int seconds_left)
{
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 4, 10, 1, (char *)"Clear BT pairings?");
    char buf[24];
    snprintf(buf, sizeof(buf), "Hold %d s...", seconds_left);
    ssd1306_draw_string(&disp, 20, 28, 1, buf);
    ssd1306_show(&disp);
}

static void handle_pairing_clear_hold(void)
{
    bool left_down = !gpio_get(DISPLAY_GPIO_BUTTON_LEFT);
    bool right_down = !gpio_get(DISPLAY_GPIO_BUTTON_RIGHT);

    if (left_down && right_down) {
        if (!pairing_clear_active) {
            pairing_clear_active = true;
            pairing_clear_started = get_absolute_time();
            pairing_clear_last_second = 5;
            show_pairing_clear_overlay(5);
        } else {
            int64_t held_ms = absolute_time_diff_us(pairing_clear_started, get_absolute_time()) / 1000;
            int seconds_left = (int)((PAIRING_CLEAR_HOLD_MS - held_ms + 999) / 1000);
            if (seconds_left < 0) {
                seconds_left = 0;
            }
            if (seconds_left != pairing_clear_last_second) {
                pairing_clear_last_second = seconds_left;
                show_pairing_clear_overlay(seconds_left);
            }
            if (held_ms >= PAIRING_CLEAR_HOLD_MS) {
                if (bluepad32_is_enabled()) {
                    bluepad32_delete_pairing_keys();
                }
                pairing_clear_active = false;
                pairing_clear_last_second = -1;
                if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
                    display_show_map_devices();
#if ADB_HOST_MODE
                } else if (current_screen == DISPLAY_SCREEN_ADB_BUS) {
                    display_show_adb_bus();
#endif
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
                    display_show_devices();
                } else {
                    display_show_splash();
                }
            }
        }
    } else if (pairing_clear_active) {
        pairing_clear_active = false;
        pairing_clear_last_second = -1;
        if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
            display_show_map_devices();
        } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
            display_show_devices();
        } else {
            display_show_splash();
        }
    }
}
#endif

void display_handle_buttons(void)
{
#if ENABLE_BLUEPAD32
    handle_pairing_clear_hold();
    if (pairing_clear_active) {
        return;
    }
#endif

    if (!gpio_get(DISPLAY_GPIO_BUTTON_MIDDLE)) {
        if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
#if ADB_HOST_MODE
                if (current_screen == DISPLAY_SCREEN_MODE) {
                    mode_screen_apply();
                } else
#endif
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
                    display_show_devices();
                } else if (current_screen == DISPLAY_SCREEN_DEVICES) {
                    display_show_map_devices();
#if ADB_HOST_MODE
                } else if (current_screen == DISPLAY_SCREEN_MAP_DEVICES) {
                    display_show_adb_bus();
                } else if (current_screen == DISPLAY_SCREEN_ADB_BUS) {
                    mode_ui_selection = adb_mode_get();
                    display_show_mode();
                } else {
                    display_show_splash();
                }
#else
                } else {
                    display_show_splash();
                }
#endif
            }
        }
    } else {
        button_middle_debounce = 0;
    }

#if ADB_HOST_MODE
    if (current_screen == DISPLAY_SCREEN_MODE) {
        if (!gpio_get(DISPLAY_GPIO_BUTTON_LEFT) && button_left_debounce == BUTTON_DEBOUNCE_COUNT) {
            mode_ui_selection = ADB_MODE_DEVICE;
            display_show_mode();
            button_left_debounce = 0;
        }
        if (!gpio_get(DISPLAY_GPIO_BUTTON_RIGHT) && button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
            mode_ui_selection = ADB_MODE_HOST;
            display_show_mode();
            button_right_debounce = 0;
        }
    }
#endif

    if (!gpio_get(DISPLAY_GPIO_BUTTON_LEFT)) {
        if (button_left_debounce <= BUTTON_DEBOUNCE_COUNT) {
            button_left_debounce++;
        }
    } else {
        button_left_debounce = 0;
    }

    if (!gpio_get(DISPLAY_GPIO_BUTTON_RIGHT)) {
        if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
            button_right_debounce++;
        }
    } else {
        button_right_debounce = 0;
    }
}
