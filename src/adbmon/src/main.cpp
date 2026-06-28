#include "adbmon_bus.h"
#include "adbmon_gpio.h"
#include "adbmon_log.h"

#include <stdio.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"

#ifndef ADBMON_VERSION_STRING
#define ADBMON_VERSION_STRING "0.2.0"
#endif

int main(void) {
    adbmon_gpio_init();
    stdio_init_all();

    uint32_t usb_wait_ms = 0;
    while (!stdio_usb_connected() && usb_wait_ms < 3000u) {
        sleep_ms(1);
        usb_wait_ms++;
    }

    adbmon_log_init();

    sleep_ms(500);
    printf("adbmon %s — passive ADB bus monitor (QuokkADB GPIO map)\n", ADBMON_VERSION_STRING);
    printf("Output: USB CDC + UART TX GPIO %d (Pico pin 1) @ %d baud\n", ADBMON_UART_TX_GPIO, ADBMON_UART_BAUD);
    printf("Tap ADB DATA on GPIO %d; bus driver released (high-Z)\n", ADBMON_ADB_IN_GPIO);
    printf("Checkpoint: GPIO %d (OLED middle) or BOOTSEL — tap / hold ~0.8s\n\n", ADBMON_BTN_GPIO);
    fflush(stdout);

    adbmon_led_blink(2);

    AdbMonBus bus;
    AdbMonFrame frame;
    absolute_time_t stats_next = make_timeout_time_ms(10000);
    bool capture_enabled = true;
    uint32_t checkpoint_id = 0;
    uint32_t frames_decoded = 0;

    while (true) {
        switch (adbmon_gpio_poll_button()) {
        case ADBMON_BTN_CHECKPOINT:
            checkpoint_id++;
            adbmon_log_marker("checkpoint", checkpoint_id, capture_enabled);
            adbmon_led_blink(1);
            break;
        case ADBMON_BTN_CAPTURE_TOGGLE:
            capture_enabled = !capture_enabled;
            adbmon_log_marker(capture_enabled ? "capture-start" : "capture-stop", checkpoint_id, capture_enabled);
            adbmon_led_blink(capture_enabled ? 2 : 3);
            break;
        default:
            break;
        }

        if (bus.poll(&frame)) {
            frames_decoded++;
            if (capture_enabled) {
                if (!adbmon_log_push(&frame)) {
                    adbmon_log_flush();
                    adbmon_log_push(&frame);
                }
            }
        }

        adbmon_log_flush();

        if (absolute_time_diff_us(get_absolute_time(), stats_next) <= 0) {
            uint32_t dropped = adbmon_log_dropped();
            uint32_t hwm = adbmon_log_high_water();
            if (dropped || hwm > (ADBMON_LOG_QUEUE_LEN / 2u) || frames_decoded == 0) {
                printf("# stats: frames=%u dropped=%u queue_hwm=%u/%u capture=%s\n",
                       frames_decoded,
                       dropped,
                       hwm,
                       ADBMON_LOG_QUEUE_LEN,
                       capture_enabled ? "ON" : "OFF");
            }
            stats_next = make_timeout_time_ms(10000);
        }
    }

    return 0;
}
