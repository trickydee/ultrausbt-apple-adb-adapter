#include "adbmon_gpio.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

// BOOTSEL shares flash CS — sample sparingly (flash/XIP + USB IRQ sensitivity).
static bool __no_inline_not_in_flash_func(adbmon_bootsel_sample)(void) {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    for (volatile int i = 0; i < 1000; ++i) {
        __nop();
    }

#ifdef __ARM_ARCH_6M__
    const uint32_t cs_bit = 1u << 1;
#else
    const uint32_t cs_bit = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#endif
    bool released = (sio_hw->gpio_hi_in & cs_bit) != 0;

    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    return !released;
}

static bool s_was_down = false;
static absolute_time_t s_down_since;
static absolute_time_t s_debounce_until;
static absolute_time_t s_last_bootsel_sample;
static bool s_bootsel_down = false;
static adbmon_btn_event_t s_pending = ADBMON_BTN_NONE;

static bool adbmon_board_button_down(void) {
    return !gpio_get(ADBMON_BTN_GPIO);
}

static bool adbmon_any_button_down(void) {
    bool down = adbmon_board_button_down();

    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(now, s_last_bootsel_sample) >= 5000) {
        s_last_bootsel_sample = now;
        s_bootsel_down = adbmon_bootsel_sample();
    }
    return down || s_bootsel_down;
}

void adbmon_gpio_init(void) {
    gpio_init(ADBMON_LED_GPIO);
    gpio_set_dir(ADBMON_LED_GPIO, GPIO_OUT);
    gpio_put(ADBMON_LED_GPIO, false);

    // Passive monitor: high-Z on ADB pins (no pulls — host provides bus pull-up).
    gpio_init(ADBMON_ADB_OUT_GPIO);
    gpio_set_dir(ADBMON_ADB_OUT_GPIO, GPIO_IN);
    gpio_disable_pulls(ADBMON_ADB_OUT_GPIO);

    gpio_init(ADBMON_ADB_IN_GPIO);
    gpio_set_dir(ADBMON_ADB_IN_GPIO, GPIO_IN);
    gpio_disable_pulls(ADBMON_ADB_IN_GPIO);

    gpio_init(ADBMON_BTN_GPIO);
    gpio_set_dir(ADBMON_BTN_GPIO, GPIO_IN);
    gpio_pull_up(ADBMON_BTN_GPIO);
}

void adbmon_led_blink(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        gpio_put(ADBMON_LED_GPIO, true);
        sleep_ms(75);
        gpio_put(ADBMON_LED_GPIO, false);
        sleep_ms(75);
    }
}

void adbmon_gpio_button_tick(void) {
    if (s_pending != ADBMON_BTN_NONE) {
        return;
    }

    bool down = adbmon_any_button_down();
    absolute_time_t now = get_absolute_time();

    if (absolute_time_diff_us(now, s_debounce_until) < 0) {
        return;
    }

    if (down && !s_was_down) {
        s_was_down = true;
        s_down_since = now;
        return;
    }

    if (!down && s_was_down) {
        s_was_down = false;
        s_debounce_until = delayed_by_ms(now, 200);
        int64_t hold_ms = -absolute_time_diff_us(now, s_down_since) / 1000;
        if (hold_ms >= 800) {
            s_pending = ADBMON_BTN_CAPTURE_TOGGLE;
        } else {
            s_pending = ADBMON_BTN_CHECKPOINT;
        }
    }
}

adbmon_btn_event_t adbmon_gpio_poll_button(void) {
    adbmon_gpio_button_tick();
    adbmon_btn_event_t event = s_pending;
    s_pending = ADBMON_BTN_NONE;
    return event;
}
