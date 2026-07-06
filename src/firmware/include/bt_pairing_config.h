/**
 * Bluetooth gamepad pairing timing — aligned with Atari v22.1.0 / Amiga v2.2.11.
 * See docs/BT_PAIRING_HANDOFF.md.
 */

#ifndef BT_PAIRING_CONFIG_H
#define BT_PAIRING_CONFIG_H

#define BT_GAMEPAD_DISCOVERY_SETTLE_MS      30
#define BT_GAMEPAD_CORE1_RESUME_DELAY_MS    100
#define BT_CORE1_PAUSE_WATCHDOG_MS          45000
#define BT_POST_READY_ADB_SETTLE_MS         2500

#endif
