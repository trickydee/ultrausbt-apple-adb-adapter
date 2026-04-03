/**
 * Display interface for SSD1306 OLED (Apple ADB / BT-USB-ADB-Adapter)
 */

#ifndef BT_USB_ADB_ADAPTER_DISPLAY_H
#define BT_USB_ADB_ADAPTER_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DISPLAY_SCREEN_SPLASH = 0,
    DISPLAY_SCREEN_DEVICES = 1,
    DISPLAY_SCREEN_BT_NAMES = 2,
} display_screen_t;

void display_init(void);
void display_show_splash(void);
void display_show_devices(void);
void display_show_bt_names(void);
void display_update_devices(void);

void display_set_usb_counts(uint8_t kb, uint8_t mouse, uint8_t joy);
void display_set_bt_counts(uint8_t kb, uint8_t mouse, uint8_t joy);

/** When a BT gamepad is connected, refresh Devices / Bluetooth names if live input state changed (throttled). */
void display_poll_bt_gamepad_viz(void);

/** Set ADB status for splash: connected, device IDs (K/M/G), srq (service request), collision. */
void display_set_adb_status(int connected, uint8_t kbd_id, uint8_t mouse_id, uint8_t game_id, int srq, int collision);

void display_handle_buttons(void);

void display_show_controller_detected(const char *controller_name, const char *controller_model, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* BT_USB_ADB_ADAPTER_DISPLAY_H */
