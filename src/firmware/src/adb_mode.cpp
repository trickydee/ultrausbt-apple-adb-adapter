#include "adb_mode.h"
#include "adb_hub.h"
#include "flashsettings.h"
#include "quokkadb_gpio.h"
#include "tusb.h"
#include <string.h>

static FlashSettings *s_settings;
static volatile adb_operating_mode_t s_mode = ADB_MODE_DEVICE;
static volatile adb_operating_mode_t s_pending_mode = ADB_MODE_DEVICE;
static volatile bool s_mode_pending = false;
static volatile bool s_usb_sync_needed = false;

static adb_operating_mode_t mode_from_flash(uint8_t b)
{
    return (b == ADB_MODE_HOST) ? ADB_MODE_HOST : ADB_MODE_DEVICE;
}

extern "C" void adb_mode_init(FlashSettings *settings)
{
    s_settings = settings;
    uint8_t stored = settings->settings()->reserved_bytes[ADB_SETTINGS_IDX_ADB_MODE];
    s_mode = mode_from_flash(stored);
    s_pending_mode = s_mode;
    s_mode_pending = false;
    s_usb_sync_needed = true;
}

extern "C" adb_operating_mode_t adb_mode_get(void)
{
    return s_mode;
}

extern "C" bool adb_mode_is_host(void)
{
    return s_mode == ADB_MODE_HOST;
}

extern "C" bool adb_mode_host_active(void)
{
    return s_mode == ADB_MODE_HOST;
}

extern "C" bool adb_mode_usb_is_device(void)
{
    return s_mode == ADB_MODE_HOST;
}

static void persist_mode(adb_operating_mode_t mode)
{
    if (!s_settings) {
        return;
    }
    QuokkADBSettings *cfg = s_settings->settings();
    cfg->reserved_bytes[ADB_SETTINGS_IDX_ADB_MODE] = (uint8_t)mode;
    s_settings->save();
}

extern "C" bool adb_mode_request(adb_operating_mode_t mode, bool persist)
{
    if (mode == s_mode && !s_mode_pending) {
        return false;
    }
    s_pending_mode = mode;
    s_mode_pending = true;
    if (persist) {
        persist_mode(mode);
    }
    return true;
}

static void usb_stack_start(adb_operating_mode_t mode)
{
    tusb_rhport_init_t init = {
        .role = (mode == ADB_MODE_HOST) ? TUSB_ROLE_DEVICE : TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO,
    };
    tusb_init(0, &init);
}

extern "C" void adb_mode_usb_sync(void)
{
    static bool usb_inited = false;
    if (!s_usb_sync_needed) {
        return;
    }
    s_usb_sync_needed = false;
    if (usb_inited) {
        tusb_deinit(0);
        busy_wait_ms(50);
    }
    usb_stack_start(s_mode);
    usb_inited = true;
    led_blink((uint8_t)(s_mode == ADB_MODE_HOST ? 2 : 1));
}

extern "C" void adb_mode_apply_pending(void)
{
    if (!s_mode_pending) {
        return;
    }
    s_mode = s_pending_mode;
    s_mode_pending = false;
    s_usb_sync_needed = true;
}
