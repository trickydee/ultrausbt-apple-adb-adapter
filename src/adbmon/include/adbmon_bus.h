#pragma once

#include <stdint.h>
#include <stdbool.h>

enum AdbMonCmdType : uint8_t {
    ADBMON_CMD_INVALID = 0,
    ADBMON_CMD_RESET,
    ADBMON_CMD_FLUSH,
    ADBMON_CMD_LISTEN,
    ADBMON_CMD_TALK,
    ADBMON_CMD_GLOBAL_RESET,
};

struct AdbMonFrame {
    uint32_t t_us;
    uint8_t raw_cmd;
    uint8_t addr;
    uint8_t reg;
    AdbMonCmdType type;
    bool srq;
    int8_t data_len;
    uint8_t data[8];
};

// Passive bus monitor — never drives the ADB line.
class AdbMonBus {
public:
    // Returns true when a decodable frame was captured.
    bool poll(AdbMonFrame *out);

private:
    int16_t recv_command(uint8_t *stop_lo_us);
    int32_t recv_register16(void);
    static AdbMonCmdType decode_type(uint8_t cmd);
    static void parse_cmd(uint8_t cmd, uint8_t *addr, uint8_t *reg, AdbMonCmdType *type);

    uint8_t data_in(void);
    uint16_t wait_data_lo(uint32_t us);
    uint16_t wait_data_hi(uint32_t us);
    bool decode_bit(uint16_t lo, uint16_t hi, uint8_t *bit_out);
};
