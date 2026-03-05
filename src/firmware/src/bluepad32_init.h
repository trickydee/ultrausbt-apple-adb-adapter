/**
 * bluepad32 initialization header for HIDHopper ADB
 */

#ifndef _BLUEPAD32_INIT_H
#define _BLUEPAD32_INIT_H

#if ENABLE_BLUEPAD32

#include <stdbool.h>
#include <pico/async_context_poll.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize Bluepad32 and return the async context (or NULL on failure)
async_context_poll_t* bluepad32_init(void);

// Poll btstack async_context (non-blocking, call from main loop)
void bluepad32_poll(void);

// Runtime control
void bluepad32_enable(void);
void bluepad32_disable(void);
bool bluepad32_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_BLUEPAD32

#endif // _BLUEPAD32_INIT_H
