#pragma once

#include "adbmon_bus.h"

#define ADBMON_LOG_QUEUE_LEN 48

void adbmon_log_init(void);
bool adbmon_log_push(const AdbMonFrame *frame);
void adbmon_log_flush(void);
void adbmon_log_marker(const char *kind, uint32_t id, bool capture_on);
uint32_t adbmon_log_dropped(void);
uint32_t adbmon_log_high_water(void);
