/**
 * See bt_host_coop.h — single volatile flag read from Core 1 USB loop, written from BT callbacks (Core 0).
 */

#include "bt_host_coop.h"

static volatile bool g_usb_host_paused;

void bt_host_coop_usb_host_set_paused(bool paused)
{
    __sync_synchronize();
    g_usb_host_paused = paused;
    __sync_synchronize();
}

bool bt_host_coop_usb_host_is_paused(void)
{
    __sync_synchronize();
    bool p = g_usb_host_paused;
    __sync_synchronize();
    return p;
}
