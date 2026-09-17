#pragma once

/**
 * Host-mode-only ADB DATA GPIO (open-collector / tri-state).
 *
 * Device mode uses drive-high via AdbInterfacePlatform in adb_platform.h.
 * Do not include this header outside adb_host.cpp.
 *
 * Archived shared 2.0.0 version: docs/archive/adb-shared-gpio-rollback.md
 */

#include "quokkadb_gpio.h"
#include "hardware/gpio.h"

namespace adb_host_gpio {

inline void bus_out(void)
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_OUT);
    gpio_put(ADB_OUT_GPIO, true);
}

inline void bus_in(void)
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_IN);
    gpio_disable_pulls(ADB_OUT_GPIO);
}

inline void data_lo(void)
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_OUT);
    ADB_OUT_LOW();
}

inline void data_hi(void)
{
    gpio_set_dir(ADB_OUT_GPIO, GPIO_IN);
    gpio_disable_pulls(ADB_OUT_GPIO);
}

} // namespace adb_host_gpio
