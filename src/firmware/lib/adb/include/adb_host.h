#pragma once

#include "adb_platform.h"
#include "adb_host_status.h"
#include <stdint.h>

/** ADB bus master — enumerates devices then polls Talk R0 (keyboard + mouse). */
class AdbHost : public AdbInterfacePlatform {
public:
    void on_enter_mode(void);
    void leave_mode(void);
    void poll(void);
    void fill_status(adb_host_status_t *out) const;
    void apply_usb_leds(uint8_t hid_leds);

private:
    enum class DevKind : uint8_t {
        Keyboard = 0,
        Pointing = 1,
    };

    struct DeviceSlot {
        uint8_t addr;
        DevKind kind;
        bool active;
        int32_t last_r0;
        int32_t last_err;
        uint32_t poll_attempts;
        uint32_t last_talk_ms;
        uint32_t last_rx_log_ms;
        uint32_t last_r2_poll_ms;
        bool ever_responded;
    };

    static constexpr uint8_t kMaxDevices = 5;
    static constexpr uint8_t kMaxPointing = 3;
    static constexpr uint32_t kRescanMs = 3000;
    static constexpr uint32_t kRescanActiveMs = 30000;
    static constexpr uint32_t kKeyboardPollMs = 8;
    static constexpr uint32_t kPointingPollMs = 24;
    static constexpr uint32_t kProbePollAttempts = 16;
    static constexpr uint32_t kRxLogMinMs = 100;
    static constexpr uint32_t kKeyboardR2PollMs = 250;

    bool send_command(uint8_t cmd);
    bool send_register16(uint16_t reg16);
    bool listen(uint8_t addr, uint8_t reg, uint16_t data16);
    int32_t receive_register16(void);
    int32_t talk(uint8_t addr, uint8_t reg);
    void global_reset(void);
    void enumerate_bus(bool reset_slots);
    void rescan_bus(void);
    void light_rescan(void);
    void scan_r3(bool verbose);
    void try_keyboard_default(void);
    void discover_pointing_devices(bool allow_relocation);
    bool try_mouse_relocation(void);
    bool ensure_device(uint8_t addr, DevKind kind);
    bool has_device_kind(DevKind kind) const;
    bool has_device_at(uint8_t addr) const;
    bool has_working_devices(void) const;
    bool slot_poll_due(const DeviceSlot *slot, uint32_t now_ms) const;
    uint8_t count_device_kind(DevKind kind) const;
    void maybe_log_summary(void);
    void log_poll_result(uint8_t addr, uint8_t reg, int32_t result, DevKind kind);
    void log_bus_wiring_test(void);
    void log_device_list(const char *label) const;
    void poll_keyboard_r2(DeviceSlot *slot, uint32_t now_ms);
    void sync_keyboard_leds_from_device(uint8_t addr);
    void poll_device_slot(DeviceSlot *slot, uint32_t now_ms);
    void prune_relocation_ghost(void);

    static const char *rx_err_label(int32_t err);

    bool place_bit0(void);
    bool place_bit1(void);
    bool send_byte(uint8_t data);
    bool decode_bit(uint16_t lo, uint16_t hi, uint8_t *bit_out);
    bool bus_wait_until_high(uint32_t timeout_us);
    bool bus_wait_until_low(uint32_t timeout_us);

    DeviceSlot devices_[kMaxDevices];
    uint8_t device_count_ = 0;
    bool entered_ = false;
    uint32_t poll_count_ = 0;
    uint32_t last_summary_ms_ = 0;
    uint32_t last_rescan_ms_ = 0;
    uint16_t last_rx_timing_ = 0;
    uint8_t last_usb_leds_ = 0xFF;
};
