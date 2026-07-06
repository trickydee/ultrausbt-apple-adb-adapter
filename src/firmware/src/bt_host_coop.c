/**
 * Refcounted Core 1 USB-host pause for BLE gamepad pairing (multicore flash race).
 * See bt_host_coop.h and docs/BT_PAIRING_PORT_AMIGA_REFERENCE.md.
 */

#include "bt_host_coop.h"
#include "bt_pairing_config.h"

#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/time.h"

volatile uint32_t g_core1_pause_spins;

static volatile uint32_t g_bt_pause_depth;
static volatile uint32_t g_core1_phase = CORE1_PHASE_RUNNING;
static uint32_t g_pause_watchdog_deadline_us;

void core1_pause_for_bt_enumeration(void)
{
    __sync_synchronize();
    if (g_bt_pause_depth == 0) {
        g_core1_pause_spins = 0;
    }
    g_bt_pause_depth++;
    if (g_bt_pause_depth == 1) {
        g_pause_watchdog_deadline_us =
            time_us_32() + (uint32_t)BT_CORE1_PAUSE_WATCHDOG_MS * 1000u;
    }
    __sync_synchronize();
}

void core1_resume_after_bt_enumeration(void)
{
    __sync_synchronize();
    if (g_bt_pause_depth > 0) {
        g_bt_pause_depth--;
    }
    if (g_bt_pause_depth == 0) {
        g_pause_watchdog_deadline_us = 0;
        g_core1_phase = CORE1_PHASE_RUNNING;
    }
    __sync_synchronize();
}

void core1_wait_for_pause_active(uint32_t timeout_ms)
{
    uint32_t deadline = time_us_32() + timeout_ms * 1000u;
    while ((int32_t)(time_us_32() - deadline) < 0) {
        __sync_synchronize();
        if (g_core1_phase == CORE1_PHASE_PAUSED || g_core1_pause_spins > 0) {
            return;
        }
        busy_wait_us(100);
    }
}

uint32_t core1_get_bt_pause_depth(void)
{
    __sync_synchronize();
    return g_bt_pause_depth;
}

void core1_force_release_bt_pause(void)
{
    __sync_synchronize();
    g_bt_pause_depth = 0;
    g_pause_watchdog_deadline_us = 0;
    __sync_synchronize();
}

bool core1_bt_pause_watchdog_tick(void)
{
    if (g_bt_pause_depth == 0) {
        return false;
    }
    if (g_pause_watchdog_deadline_us != 0 &&
        (int32_t)(time_us_32() - g_pause_watchdog_deadline_us) >= 0) {
        core1_force_release_bt_pause();
        return true;
    }
    return false;
}

bool bt_host_coop_usb_host_is_paused(void)
{
    __sync_synchronize();
    return g_bt_pause_depth > 0;
}

void bt_host_coop_core1_pause_iteration(void)
{
    g_core1_phase = CORE1_PHASE_PAUSED;
    g_core1_pause_spins++;
    __wfe();
}

void bt_host_coop_core1_set_running(void)
{
    g_core1_phase = CORE1_PHASE_RUNNING;
}
