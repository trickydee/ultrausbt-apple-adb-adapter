/**
 * bluepad32 initialization for HIDHopper ADB
 * Separate from main to avoid HID type conflicts between TinyUSB and btstack
 */

#if ENABLE_BLUEPAD32

#include <stdio.h>
#include <pico/cyw43_arch.h>
#include <pico/async_context.h>
#include <pico/async_context_poll.h>
#include <pico/btstack_run_loop_async_context.h>
#include <pico/time.h>
#include <hardware/gpio.h>
#include <uni.h>
#include "bluepad32_platform.h"

static async_context_poll_t* g_btstack_async_context = NULL;
static bool g_bluetooth_enabled = false;

async_context_poll_t* bluepad32_init(void) {
    if (g_bluetooth_enabled) {
        return g_btstack_async_context;
    }
    static async_context_poll_t btstack_async_context;

    if (!async_context_poll_init_with_defaults(&btstack_async_context)) {
        return NULL;
    }

    cyw43_arch_set_async_context(&btstack_async_context.core);

#ifdef CYW43_DEFAULT_PIN_WL_REG_ON
    gpio_init(CYW43_DEFAULT_PIN_WL_REG_ON);
    gpio_set_dir(CYW43_DEFAULT_PIN_WL_REG_ON, GPIO_OUT);
    gpio_put(CYW43_DEFAULT_PIN_WL_REG_ON, 0);
    sleep_ms(100);
    gpio_put(CYW43_DEFAULT_PIN_WL_REG_ON, 1);
    sleep_ms(250);
#endif

    int cyw43_result = cyw43_arch_init();
    if (cyw43_result) {
        async_context_deinit(&btstack_async_context.core);
        return NULL;
    }

    uni_platform_set_custom(get_my_platform());

    const btstack_run_loop_t* btstack_run_loop =
        btstack_run_loop_async_context_get_instance(&btstack_async_context.core);
    btstack_run_loop_init(btstack_run_loop);

    uni_init(0, NULL);

    g_btstack_async_context = &btstack_async_context;
    g_bluetooth_enabled = true;
    return &btstack_async_context;
}

void bluepad32_deinit(void) {
    if (!g_bluetooth_enabled) return;
    if (g_btstack_async_context) {
        cyw43_arch_deinit();
        async_context_deinit(&g_btstack_async_context->core);
        g_btstack_async_context = NULL;
    }
    g_bluetooth_enabled = false;
}

void bluepad32_enable(void) {
    if (!g_bluetooth_enabled) bluepad32_init();
}

void bluepad32_disable(void) {
    if (g_bluetooth_enabled) bluepad32_deinit();
}

bool bluepad32_is_enabled(void) {
    return g_bluetooth_enabled;
}

void bluepad32_poll(void) {
    if (g_btstack_async_context) {
        async_context_poll(&g_btstack_async_context->core);
    }
}

#endif // ENABLE_BLUEPAD32
