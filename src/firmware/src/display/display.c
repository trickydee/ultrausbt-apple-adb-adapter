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

void display_show_splash(void)
{
    ssd1306_clear(&disp);

    /* Title: Apple ADB (replacing Amiga) */
    ssd1306_draw_string(&disp, 15, 0, 2, (char *)"Apple ADB");

    ssd1306_draw_string(&disp, 2, 24, 1, (char *)"HIDHopper ADB");

    /* Version */
    ssd1306_draw_string(&disp, 35, 40, 1, (char *)"v" HIDHOPPER_ADB_VERSION_STRING);

#if ENABLE_BLUEPAD32
    bool bt_enabled = bluepad32_is_enabled();
    if (bt_enabled)
        ssd1306_draw_string(&disp, 0, 55, 1, (char *)"Mode USB+BT  RST");
    else
        ssd1306_draw_string(&disp, 0, 55, 1, (char *)"Mode USB");
#else
    ssd1306_draw_string(&disp, 0, 55, 1, (char *)"Mode USB");
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_SPLASH;
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

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_DEVICES;
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
    display_update_devices();
}

void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy)
{
    (void)joy;
    bt_kb_count = kb;
    bt_mouse_count = mouse;
    display_update_devices();
    if (current_screen == DISPLAY_SCREEN_SPLASH)
        display_show_splash();
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

    /* Right: on splash with BT = clear pairings */
    if (!gpio_get(DISPLAY_GPIO_BUTTON_RIGHT)) {
        if (button_right_debounce <= BUTTON_DEBOUNCE_COUNT) {
            if (++button_right_debounce == BUTTON_DEBOUNCE_COUNT) {
                if (current_screen == DISPLAY_SCREEN_SPLASH) {
#if ENABLE_BLUEPAD32
                    if (bluepad32_is_enabled()) {
                        bluepad32_delete_pairing_keys();
                        display_show_splash();
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
#else
    ssd1306_draw_string(&disp, 0, 0, 1, (char *)"BT not enabled");
#endif

    ssd1306_show(&disp);
    current_screen = DISPLAY_SCREEN_BT_NAMES;
}
