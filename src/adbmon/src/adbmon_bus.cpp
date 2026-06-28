#include "adbmon_bus.h"

#include "adbmon_gpio.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#ifndef ADB_ATTENTION_LO_MIN_US
#define ADB_ATTENTION_LO_MIN_US 500
#endif

#ifndef ADBMON_STRICT_DUTY_CYCLE_DECODE
#define ADBMON_STRICT_DUTY_CYCLE_DECODE 0
#endif

#ifndef ADBMON_STRICT_SYNC_WINDOW
#define ADBMON_STRICT_SYNC_WINDOW 0
#endif

// Match QuokkADB adb.cpp: long window when bus is active, short when idle.
#ifndef ADBMON_START_BIT_WAIT_ACTIVE_US
#define ADBMON_START_BIT_WAIT_ACTIVE_US 100000u
#endif
#ifndef ADBMON_START_BIT_WAIT_IDLE_US
#define ADBMON_START_BIT_WAIT_IDLE_US 2000u
#endif
#ifndef ADBMON_BUS_ACTIVE_HOLD_US
#define ADBMON_BUS_ACTIVE_HOLD_US 50000u
#endif

static uint64_t s_last_decode_ok_us = 0;

uint8_t AdbMonBus::data_in(void) {
    return gpio_get(ADBMON_ADB_IN_GPIO) ? 1u : 0u;
}

uint16_t AdbMonBus::wait_data_lo(uint32_t us) {
    uint64_t start = time_us_64();
    uint64_t time = start;
    do {
        if (!data_in()) {
            time = time_us_64();
            break;
        }
        time = time_us_64();
    } while (us >= time - start);
    uint16_t diff = static_cast<uint16_t>(time - start);
    return us >= diff ? diff : 0;
}

uint16_t AdbMonBus::wait_data_hi(uint32_t us) {
    uint64_t start = time_us_64();
    uint64_t time = start;
    do {
        if (data_in()) {
            time = time_us_64();
            break;
        }
        time = time_us_64();
    } while (us >= time - start);
    return static_cast<uint16_t>(time - start);
}

bool AdbMonBus::decode_bit(uint16_t lo, uint16_t hi, uint8_t *bit_out) {
    uint16_t cell = static_cast<uint16_t>(lo + hi);
    if (cell < 70 || cell > 130) {
        return false;
    }
#if ADBMON_STRICT_DUTY_CYCLE_DECODE
    uint32_t lo_x100 = static_cast<uint32_t>(lo) * 100u;
    uint32_t cell_x35 = 35u * static_cast<uint32_t>(cell);
    uint32_t cell_x65 = 65u * static_cast<uint32_t>(cell);
    if (lo_x100 < cell_x35) {
        *bit_out = 1;
    } else if (lo_x100 > cell_x65) {
        *bit_out = 0;
    } else {
        return false;
    }
#else
    *bit_out = (static_cast<uint32_t>(lo) * 100u < 50u * static_cast<uint32_t>(cell)) ? 1u : 0u;
#endif
    return true;
}

AdbMonCmdType AdbMonBus::decode_type(uint8_t cmd) {
    switch ((cmd & 0x0Cu) >> 2) {
    case 0x0:
        return (cmd & 0x01u) ? ADBMON_CMD_FLUSH : ADBMON_CMD_RESET;
    case 0x2:
        return ADBMON_CMD_LISTEN;
    case 0x3:
        return ADBMON_CMD_TALK;
    default:
        return ADBMON_CMD_INVALID;
    }
}

void AdbMonBus::parse_cmd(uint8_t cmd, uint8_t *addr, uint8_t *reg, AdbMonCmdType *type) {
    *reg = cmd & 0x03u;
    *addr = (cmd >> 4) & 0x0Fu;
    *type = decode_type(cmd);
}

int16_t AdbMonBus::recv_command(uint8_t *stop_lo_us) {
    uint8_t bits = 0;
    uint16_t lo = 0;
    uint16_t hi = 0;
    int16_t data = 0;

    uint64_t now = time_us_64();
    uint32_t attention_wait = (now - s_last_decode_ok_us < ADBMON_BUS_ACTIVE_HOLD_US)
                                  ? ADBMON_START_BIT_WAIT_ACTIVE_US
                                  : ADBMON_START_BIT_WAIT_IDLE_US;
    hi = wait_data_lo(attention_wait);
    if (!hi) {
        return -1;
    }

    do {
        lo = wait_data_hi(4000);
        if (!lo) {
            return -1;
        }
        if (lo >= 2800) {
            // Floating/disconnected bus can sit low for the full timeout; require release.
            if (!data_in()) {
                return -1;
            }
            return -100;
        }
        if (lo > 1040 || lo < ADB_ATTENTION_LO_MIN_US) {
            return -1;
        }
        break;
    } while (true);

    hi = wait_data_lo(150);
#if ADBMON_STRICT_SYNC_WINDOW
    if (!hi || hi > 91 || hi < 42) {
#else
    if (!hi || hi > 95 || hi < 40) {
#endif
        return -3;
    }

    for (bits = 0; bits < 8; bits++) {
        lo = wait_data_hi(130);
        if (!lo) {
            return -4;
        }
        hi = wait_data_lo(100);
        if (!hi) {
            return -4;
        }
        uint8_t bit = 0;
        if (!decode_bit(lo, hi, &bit)) {
            return -4;
        }
        data = static_cast<int16_t>((data << 1) | bit);
    }

    hi = wait_data_hi(400);
    *stop_lo_us = static_cast<uint8_t>(hi > 255 ? 255 : hi);
    return data;
}

int32_t AdbMonBus::recv_register16(void) {
    int32_t data = 0;
    uint16_t hi_time = wait_data_lo(1000);
    if (!hi_time || hi_time < 140 || hi_time > 260) {
        return -1;
    }

    uint16_t low_time = wait_data_hi(130);
    if (!low_time || low_time > 55 || low_time < 18) {
        return -2;
    }

    hi_time = wait_data_lo(130);
    if (!hi_time || hi_time > 90 || hi_time < 40) {
        return -3;
    }

    for (uint8_t bits = 0; bits < 16; bits++) {
        uint16_t lo = wait_data_hi(130);
        if (!lo) {
            return -5;
        }
        uint16_t hi = wait_data_lo(100);
        if (!hi) {
            return -5;
        }
        uint8_t bit = 0;
        if (!decode_bit(lo, hi, &bit)) {
            return -5;
        }
        data = (data << 1) | bit;
    }

    low_time = wait_data_hi(130);
    if (!low_time || low_time > 85) {
        return -4;
    }

    return data;
}

bool AdbMonBus::poll(AdbMonFrame *out) {
    static uint32_t last_global_reset_us = 0;

    uint8_t stop_lo = 0;
    int16_t cmd = recv_command(&stop_lo);
    if (cmd < 0) {
        if (cmd == -100) {
            uint32_t now_us = static_cast<uint32_t>(time_us_64());
            // Open/floating bus can look like multi-ms lows; debounce GlobalReset reports.
            if (now_us - last_global_reset_us < 100000u) {
                return false;
            }
            last_global_reset_us = now_us;
            out->t_us = now_us;
            out->raw_cmd = 0;
            out->addr = 0;
            out->reg = 0;
            out->type = ADBMON_CMD_GLOBAL_RESET;
            out->srq = false;
            out->data_len = 0;
            return true;
        }
        return false;
    }

    out->t_us = static_cast<uint32_t>(time_us_64());
    out->raw_cmd = static_cast<uint8_t>(cmd);
    parse_cmd(out->raw_cmd, &out->addr, &out->reg, &out->type);
    out->srq = stop_lo >= 120;
    out->data_len = 0;

    if (out->type == ADBMON_CMD_TALK || out->type == ADBMON_CMD_LISTEN) {
        int32_t reg16 = recv_register16();
        if (reg16 >= 0) {
            out->data[0] = static_cast<uint8_t>((reg16 >> 8) & 0xFF);
            out->data[1] = static_cast<uint8_t>(reg16 & 0xFF);
            out->data_len = 2;
        }
    }

    s_last_decode_ok_us = time_us_64();
    return true;
}
