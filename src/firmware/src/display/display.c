/**
 * Display interface for SSD1306 OLED – Apple ADB / HIDHopper ADB
 * Adapted from amigahid-pico display; Amiga references replaced with Apple ADB.
 */

#include "display/display.h"
#include "display/display_config.h"
#include "ssd1306.h"
#include <hardware/i2c.h>
#include <hardware/gpio.h>
#include <pico/time.h>
#include <stdio.h>
#include <string.h>

#if ENABLE_BLUEPAD32
#include "bluepad32_init.h"
#include "bluepad32_platform.h"  /* bluepad32_delete_pairing_keys, bluepad32_get_device_name */
#endif

/* Version string from CMake (e.g. "1.0.2") */
#ifndef HIDHOPPER_ADB_VERSION_STRING
#define HIDHOPPER_ADB_VERSION_STRING "?.?.?"
#endif

static ssd1306_t disp;

/* Device counts (set from USB/BT; only core0 reads for drawing) */
static volatile uint8_t usb_kb_count = 0;
static volatile uint8_t usb_mouse_count = 0;
static volatile uint8_t bt_kb_count = 0;
static volatile uint8_t bt_mouse_count = 0;
static volatile uint8_t bt_joy_count = 0;

/* Last values we actually drew; only refresh display when these change */
static uint8_t last_drawn_usb_kb = 0xFF;
static uint8_t last_drawn_usb_mouse = 0xFF;
static uint8_t last_drawn_bt_kb = 0xFF;
static uint8_t last_drawn_bt_mouse = 0xFF;
static uint8_t last_drawn_bt_joy = 0xFF;

/* ADB status for splash (set from main loop) */
static int adb_connected = 0;
static uint8_t adb_kbd_id = 0;
static uint8_t adb_mouse_id = 0;
static uint8_t adb_game_id = 0;
static int adb_srq = 0;      /* service request (kbd or mouse has data pending) */
static int adb_collision = 0;
static int last_drawn_adb_connected = -1;
static uint8_t last_drawn_adb_kbd = 0xFF;
static uint8_t last_drawn_adb_mouse = 0xFF;
static uint8_t last_drawn_adb_game = 0xFF;
static int last_drawn_adb_srq = -1;
static int last_drawn_adb_collision = -1;

static display_screen_t current_screen = DISPLAY_SCREEN_SPLASH;

#define BUTTON_DEBOUNCE_COUNT 10
static uint8_t button_middle_debounce = 0;
static uint8_t button_left_debounce = 0;
static uint8_t button_right_debounce = 0;

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
    if (current_screen != DISPLAY_SCREEN_SPLASH)
        return;
    /* Redraw only when connection or device IDs change; not on SRQ/collision to avoid
     * full-screen I2C refresh during mouse movement. S/! are still drawn when we
     * redraw for other reasons. */
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

    /* Title: Apple - original font size (scale 2), centered. 5*16=80, (128-80)/2=24 */
    ssd1306_draw_string(&disp, 24, 0, 2, (char *)"Apple");

    ssd1306_draw_string(&disp, 2, 24, 1, (char *)"HIDHopper ADB");

    /* Version */
    ssd1306_draw_string(&disp, 35, 40, 1, (char *)"v" HIDHOPPER_ADB_VERSION_STRING);

    /* ADB status line: K M G, then S=SRQ (service request), !=collision */
    if (!adb_connected) {
        ssd1306_draw_string(&disp, 0, 55, 1, (char *)"ADB: --");
    } else {
        snprintf(line, sizeof(line), "ADB: K%X M%X G%X%s%s",
                (unsigned)adb_kbd_id, (unsigned)adb_mouse_id, (unsigned)adb_game_id,
                adb_srq ? " S" : "", adb_collision ? "!" : "");
        ssd1306_draw_string(&disp, 0, 55, 1, line);
    }

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SPLASH;
    last_drawn_bt_kb = bt_kb_count;
    last_drawn_bt_mouse = bt_mouse_count;
    last_drawn_bt_joy = bt_joy_count;
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

    sprintf(buf, "Keybd   U %d BT %d", (int)usb_kb_count, (int)bt_kb_count);
    ssd1306_draw_string(&disp, 0, 9, 1, buf);

    sprintf(buf, "Mouse   U %d BT %d", (int)usb_mouse_count, (int)bt_mouse_count);
    ssd1306_draw_string(&disp, 0, 18, 1, buf);

    sprintf(buf, "Gamepad U 0 BT %d", (int)bt_joy_count);
    ssd1306_draw_string(&disp, 0, 27, 1, buf);

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_DEVICES;
    last_drawn_usb_kb = usb_kb_count;
    last_drawn_usb_mouse = usb_mouse_count;
    last_drawn_bt_kb = bt_kb_count;
    last_drawn_bt_mouse = bt_mouse_count;
    last_drawn_bt_joy = bt_joy_count;
}

void display_update_devices(void)
{
    if (current_screen == DISPLAY_SCREEN_DEVICES)
        display_show_devices();
    if (current_screen == DISPLAY_SCREEN_BT_NAMES)
        display_show_bt_names();
}

void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    (void)joy;
    usb_kb_count = kb;
    usb_mouse_count = mouse;
    /* Only redraw when counts changed to avoid hammering I2C every main-loop iteration */
    if (last_drawn_usb_kb != kb || last_drawn_usb_mouse != mouse) {
        last_drawn_usb_kb = kb;
        last_drawn_usb_mouse = mouse;
        display_update_devices();
    }
}

void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    bt_kb_count = kb;
    bt_mouse_count = mouse;
    bt_joy_count = joy;
    /* Only redraw when counts changed to avoid hammering I2C every main-loop iteration */
    if (last_drawn_bt_kb != kb || last_drawn_bt_mouse != mouse || last_drawn_bt_joy != joy) {
        last_drawn_bt_kb = kb;
        last_drawn_bt_mouse = mouse;
        last_drawn_bt_joy = joy;
        display_update_devices();
        if (current_screen == DISPLAY_SCREEN_SPLASH)
            display_show_splash();
    }
}

void display_show_controller_detected(const char *controller_name, const char *controller_model, uint32_t duration_ms)
{
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 25, 10, 2, (char *)controller_name);
    if (controller_model)
        ssd1306_draw_string(&disp, 10, 35, 1, (char *)controller_model);
    ssd1306_show(&disp);
    sleep_ms(duration_ms);
    display_show_splash();
}

void display_handle_buttons(void)
{
    /* Middle: cycle SPLASH -> DEVICES -> BT_NAMES -> SPLASH */
    if (!gpio_get(DISPLAY_GPIO_BUTTON_MIDDLE)) {
        if (button_middle_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_middle_debounce == BUTTON_DEBOUNCE_COUNT) {
                if (current_screen == DISPLAY_SCREEN_SPLASH)
                    display_show_devices();
                else if (current_screen == DISPLAY_SCREEN_DEVICES) {
#if ENABLE_BLUEPAD32
                    display_show_bt_names();
#else
                    display_show_splash();
#endif
                } else
                    display_show_splash();
            }
        }
    } else {
        button_middle_debounce = 0;
    }

    /* Right: on Bluetooth devices page = clear BT pairings */
    if (!gpio_get(DISPLAY_GPIO_BUTTON_RIGHT)) {
        if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
                if (current_screen == DISPLAY_SCREEN_BT_NAMES) {
#if ENABLE_BLUEPAD32
                    if (bluepad32_is_enabled()) {
                        bluepad32_delete_pairing_keys();
                        display_show_bt_names();
                    }
#endif
                }
            }
        }
    } else {
        button_right_debounce = 0;
    }

    /* Left: reserved / no action for now */
    if (!gpio_get(DISPLAY_GPIO_BUTTON_LEFT)) {
        if (button_left_debounce <= BUTTON_DEBOUNCE_COUNT)
            button_left_debounce++;
    } else {
        button_left_debounce = 0;
    }
}

void display_show_bt_names(void)
{
    ssd1306_clear(&disp);

#if ENABLE_BLUEPAD32
    char buf[32];
    const char *name;

    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"Bluetooth Devices");

    name = bluepad32_get_device_name('K', 0);
    if (name) {
        snprintf(buf, sizeof(buf), "K1:%.20s", name);
        ssd1306_draw_string(&disp, 0, 9, 1, buf);
    } else
        ssd1306_draw_string(&disp, 0, 9, 1, (char *)"K1: --");

    name = bluepad32_get_device_name('M', 0);
    if (name) {
        snprintf(buf, sizeof(buf), "M1:%.20s", name);
        ssd1306_draw_string(&disp, 0, 18, 1, buf);
    } else
        ssd1306_draw_string(&disp, 0, 18, 1, (char *)"M1: --");

    name = bluepad32_get_device_name('G', 0);
    if (name) {
        snprintf(buf, sizeof(buf), "G1:%.20s", name);
        ssd1306_draw_string(&disp, 0, 27, 1, buf);
    } else
        ssd1306_draw_string(&disp, 0, 27, 1, (char *)"G1: --");

    ssd1306_draw_string(&disp, 0, 55, 1, (char *)"R: clear pairings");
#else
    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"BT not enabled");
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_BT_NAMES;
    last_drawn_bt_kb = bt_kb_count;
    last_drawn_bt_mouse = bt_mouse_count;
    last_drawn_bt_joy = bt_joy_count;
}
