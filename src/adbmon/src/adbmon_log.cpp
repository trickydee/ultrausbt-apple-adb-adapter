#include "adbmon_log.h"

#include <stdio.h>
#include <string.h>

#include "pico/time.h"

static AdbMonFrame s_queue[ADBMON_LOG_QUEUE_LEN];
static volatile uint16_t s_head = 0;
static volatile uint16_t s_tail = 0;
static volatile uint32_t s_dropped = 0;
static volatile uint32_t s_high_water = 0;

void adbmon_log_init(void) {
    s_head = 0;
    s_tail = 0;
    s_dropped = 0;
    s_high_water = 0;
}

static const char *cmd_name(AdbMonCmdType type) {
    switch (type) {
    case ADBMON_CMD_RESET:
        return "Reset";
    case ADBMON_CMD_FLUSH:
        return "Flush";
    case ADBMON_CMD_LISTEN:
        return "Listen";
    case ADBMON_CMD_TALK:
        return "Talk";
    case ADBMON_CMD_GLOBAL_RESET:
        return "GlobalReset";
    default:
        return "Invalid";
    }
}

static void print_frame(const AdbMonFrame *frame) {
    if (frame->type == ADBMON_CMD_GLOBAL_RESET) {
        printf("ADB evt=GlobalReset t_us=%u\n", frame->t_us);
        return;
    }

    printf("ADB evt=%s raw=%02X addr=%X reg=%u srq=%u",
           cmd_name(frame->type),
           frame->raw_cmd,
           frame->addr,
           frame->reg,
           frame->srq ? 1u : 0u);

    if (frame->data_len > 0) {
        printf(" data=%d:", frame->data_len);
        for (int i = 0; i < frame->data_len; i++) {
            printf("%02X", frame->data[i]);
        }
    } else {
        printf(" data=-");
    }

    printf(" t_us=%u\n", frame->t_us);
}

bool adbmon_log_push(const AdbMonFrame *frame) {
    uint16_t next = static_cast<uint16_t>((s_head + 1u) % ADBMON_LOG_QUEUE_LEN);
    if (next == s_tail) {
        s_dropped++;
        return false;
    }
    s_queue[s_head] = *frame;
    s_head = next;
    uint16_t depth = static_cast<uint16_t>((s_head + ADBMON_LOG_QUEUE_LEN - s_tail) % ADBMON_LOG_QUEUE_LEN);
    if (depth > s_high_water) {
        s_high_water = depth;
    }
    return true;
}

void adbmon_log_flush(void) {
    while (s_tail != s_head) {
        print_frame(&s_queue[s_tail]);
        s_tail = static_cast<uint16_t>((s_tail + 1u) % ADBMON_LOG_QUEUE_LEN);
    }
    fflush(stdout);
}

void adbmon_log_marker(const char *kind, uint32_t id, bool capture_on) {
    adbmon_log_flush();
    printf("# adbmon %s id=%u capture=%s t_us=%u\n",
           kind,
           id,
           capture_on ? "ON" : "OFF",
           static_cast<uint32_t>(time_us_64()));
    fflush(stdout);
}

uint32_t adbmon_log_dropped(void) {
    return s_dropped;
}

uint32_t adbmon_log_high_water(void) {
    return s_high_water;
}
