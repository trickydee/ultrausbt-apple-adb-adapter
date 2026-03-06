/**
 * Bluetooth pairing sync: flash-safe core init and core1 pause during pairing.
 * Matches approach used in amigahid-pico to avoid hangs (timing, flash, core pause).
 */

#if ENABLE_BLUEPAD32

#include "bt_pairing_sync.h"
#include "pico/flash.h"
#include <stdbool.h>
#include <stdint.h>

static volatile bool s_core1_paused = false;

void bt_pairing_sync_core1_init(void) {
    /* Allow Core 0 to coordinate with this core when btstack writes TLV to flash during pairing.
     * Without this, Core 1 can freeze when Bluetooth accesses flash (e.g. identity/bond storage). */
    flash_safe_execute_core_init();
}

bool bt_pairing_sync_is_core1_paused(void) {
    return s_core1_paused;
}

void bt_pairing_sync_pause_core1(void) {
    __sync_synchronize();
    s_core1_paused = true;
    __sync_synchronize();
}

void bt_pairing_sync_resume_core1(void) {
    __sync_synchronize();
    s_core1_paused = false;
    __sync_synchronize();
}

#endif /* ENABLE_BLUEPAD32 */
