/**
 * Core 0 (Bluetooth / ADB) ↔ Core 1 (USB host) cooperation.
 * Refcounted pause during BLE gamepad enumeration so Core 1 stops calling
 * tuh_task() from XIP while BTstack writes pairing TLV via flash_safe_execute().
 * Pattern from ultramegausb-atari-st-rpikbd v22.1.0 / ultramegausb-amiga.
 */

#ifndef BT_HOST_COOP_H
#define BT_HOST_COOP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CORE1_PHASE_RUNNING = 0,
    CORE1_PHASE_PAUSED = 1,
};

/** Incremented by Core 1 each iteration in the pause branch (diagnostics / wait_for_pause). */
extern volatile uint32_t g_core1_pause_spins;

void core1_pause_for_bt_enumeration(void);
void core1_resume_after_bt_enumeration(void);
void core1_wait_for_pause_active(uint32_t timeout_ms);
uint32_t core1_get_bt_pause_depth(void);
void core1_force_release_bt_pause(void);
bool core1_bt_pause_watchdog_tick(void);

/** True when BT pause depth > 0 — Core 1 should __wfe() instead of tuh_task(). */
bool bt_host_coop_usb_host_is_paused(void);

/** Called from Core 1 pause branch each iteration. */
void bt_host_coop_core1_pause_iteration(void);

/** Called from Core 1 when running tuh_task (not paused). */
void bt_host_coop_core1_set_running(void);

#ifdef __cplusplus
}
#endif

#endif
