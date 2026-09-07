#pragma once

#include <stdint.h>

// ultrausbt DIY board GPIO map (passive monitor uses IN only — see docs/hardware.md)
#define ADBMON_LED_GPIO     25
#define ADBMON_ADB_IN_GPIO  19
#define ADBMON_ADB_OUT_GPIO 18
#define ADBMON_BTN_GPIO     7   // QuokkADB OLED middle button (active low); BOOTSEL also works
#define ADBMON_UART_TX_GPIO 0
#define ADBMON_UART_RX_GPIO 1
#define ADBMON_UART_BAUD    115200

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADBMON_BTN_NONE = 0,
    ADBMON_BTN_CHECKPOINT,       /* short BOOTSEL tap */
    ADBMON_BTN_CAPTURE_TOGGLE,   /* hold BOOTSEL ~0.8 s */
} adbmon_btn_event_t;

void adbmon_gpio_init(void);
void adbmon_led_blink(uint8_t times);
void adbmon_gpio_button_tick(void);
adbmon_btn_event_t adbmon_gpio_poll_button(void);

#ifdef __cplusplus
}
#endif
