/**
 * Minimal Bluepad32 API for app (no Pico/btstack includes).
 * Use this from C++ app code to avoid pulling in pico/async_context_poll.h.
 */
#ifndef _BLUEPAD32_API_H
#define _BLUEPAD32_API_H

#if ENABLE_BLUEPAD32

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns non-NULL on success (opaque async context)
void* bluepad32_init(void);
void bluepad32_poll(void);
void bluepad32_enable(void);
void bluepad32_disable(void);
bool bluepad32_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // ENABLE_BLUEPAD32

#endif // _BLUEPAD32_API_H
