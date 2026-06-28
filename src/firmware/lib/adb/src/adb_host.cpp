#include "adb_host.h"
#include "adb_to_usb.h"
#include "adbregisters.h"
#include "quokkadb_gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <stdlib.h>

#if QUOKKADB
#include "rp2040_serial.h"
using rp2040_serial::Serial;
#endif

extern bool global_debug;

static bool addr_is_keyboard_slot(uint8_t addr)
{
    return addr == 0x02 || addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x07;
}

static bool addr_is_pointing_slot(uint8_t addr)
{
    return addr == 0x03 || addr == 0x0E || addr == 0x0F;
}

static bool classify_r3_valid(uint8_t addr, uint8_t handler, bool *is_keyboard_out)
{
    if (handler == 0xFE || handler == 0xFF) {
        return false;
    }
    if (addr_is_keyboard_slot(addr)) {
        *is_keyboard_out = true;
        return true;
    }
    if (addr_is_pointing_slot(addr)) {
        *is_keyboard_out = false;
        return true;
    }
    return false;
}

/** Addresses to probe during enumeration (Quadra 700 trackball uses 0x0F after relocation). */
static const uint8_t kScanAddrs[] = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0E, 0x0F};
static const unsigned kScanCount = sizeof(kScanAddrs) / sizeof(kScanAddrs[0]);

const char *AdbHost::rx_err_label(int32_t err)
{
    switch (err) {
    case -1:
        return "Tlt";
    case -2:
        return "start";
    case -3:
        return "sync";
    case -4:
        return "stop";
    case -5:
        return "BIT";
    case -10:
        return "TX";
    default:
        return "?";
    }
}

bool AdbHost::place_bit0(void)
{
    data_lo();
    adb_delay_us(65);
    data_hi();
    return adb_delay_us(35);
}

bool AdbHost::place_bit1(void)
{
    data_lo();
    adb_delay_us(35);
    data_hi();
    return adb_delay_us(65);
}

bool AdbHost::send_byte(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        if (data & (0x80 >> i)) {
            if (!place_bit1()) {
                return false;
            }
        } else {
            if (!place_bit0()) {
                return false;
            }
        }
    }
    return true;
}

bool AdbHost::decode_bit(uint16_t lo, uint16_t hi, uint8_t *bit_out)
{
    uint16_t cell = (uint16_t)(lo + hi);
    if (cell < 70 || cell > 130) {
        return false;
    }
    *bit_out = ((uint32_t)lo * 100u < 50u * (uint32_t)cell) ? 1u : 0u;
    return true;
}

bool AdbHost::send_command(uint8_t cmd)
{
    adb_pin_out();
    data_lo();
    // TMK/QMK attention: 800 µs low total; place_bit1() adds the final 35 µs low (start bit).
    if (!adb_delay_us(765)) {
        adb_pin_in();
        return false;
    }
    if (!place_bit1()) {
        adb_pin_in();
        return false;
    }
    if (!send_byte(cmd)) {
        adb_pin_in();
        return false;
    }
    if (!place_bit0()) {
        adb_pin_in();
        return false;
    }
    adb_pin_in();
    return true;
}

void AdbHost::log_bus_wiring_test(void)
{
    if (!global_debug) {
        return;
    }

    adb_pin_out();
    data_hi();
    busy_wait_us(200);
    uint8_t in_idle = data_in();

    data_lo();
    busy_wait_us(500);
    uint8_t in_lo = data_in();

    data_hi();
    busy_wait_us(500);
    uint8_t in_hi = data_in();
    adb_pin_in();

    Serial.print("ADB host: bus test GP");
    Serial.print(ADB_OUT_GPIO, DEC);
    Serial.print("->GP");
    Serial.print(ADB_IN_GPIO, DEC);
    Serial.print(" idle=");
    Serial.print(in_idle ? "hi" : "lo");
    Serial.print(" drv_lo=");
    Serial.print(in_lo ? "hi" : "lo");
    Serial.print(" drv_hi=");
    Serial.println(in_hi ? "hi" : "lo");
}

int32_t AdbHost::receive_register16(void)
{
    int32_t data = 0;

    // QMK adb_host_talk_buf: wait for bus high after command stop (skip SRQ stretch).
    if (!wait_data_hi(500)) {
        last_rx_timing_ = 0;
        return -1;
    }
    if (!wait_data_lo(500)) {
        last_rx_timing_ = 0;
        return -1;
    }

    // Start bit (1)
    uint16_t low_time = wait_data_hi(130);
    if (!low_time || low_time > 55 || low_time < 18) {
        last_rx_timing_ = low_time;
        return -2;
    }

    uint16_t hi_time = wait_data_lo(130);
    if (!hi_time || hi_time > 90 || hi_time < 40) {
        last_rx_timing_ = hi_time;
        return -3;
    }

    for (uint8_t bits = 0; bits < 16; bits++) {
        uint16_t lo = wait_data_hi(130);
        if (!lo) {
            last_rx_timing_ = 0;
            return -5;
        }
        uint16_t hi = wait_data_lo(100);
        if (!hi) {
            last_rx_timing_ = 0;
            return -5;
        }
        uint8_t bit = 0;
        if (!decode_bit(lo, hi, &bit)) {
            last_rx_timing_ = (uint16_t)(lo + hi);
            return -5;
        }
        data = (data << 1) | bit;
    }

    low_time = wait_data_hi(130);
    if (!low_time || low_time > 85) {
        last_rx_timing_ = low_time;
        return -4;
    }

    return data;
}

bool AdbHost::send_register16(uint16_t reg16)
{
    uint32_t extra_delay = (uint32_t)(rand() % 101);
    if (!adb_delay_us(140 + extra_delay)) {
        return false;
    }

    adb_pin_out();
    if (!place_bit1()) {
        adb_pin_in();
        return false;
    }
    if (!send_byte((uint8_t)((reg16 >> 8) & 0xFF))) {
        adb_pin_in();
        return false;
    }
    if (!send_byte((uint8_t)(reg16 & 0xFF))) {
        adb_pin_in();
        return false;
    }
    if (!place_bit0()) {
        adb_pin_in();
        return false;
    }
    adb_pin_in();
    return true;
}

bool AdbHost::listen(uint8_t addr, uint8_t reg, uint16_t data16)
{
    uint8_t cmd = (uint8_t)((addr << 4) | 0x08 | (reg & 0x03));
    if (!send_command(cmd)) {
        return false;
    }
    return send_register16(data16);
}

int32_t AdbHost::talk(uint8_t addr, uint8_t reg)
{
    uint8_t cmd = (uint8_t)((addr << 4) | 0x0C | (reg & 0x03));
    if (!send_command(cmd)) {
        return -10;
    }
    return receive_register16();
}

void AdbHost::global_reset(void)
{
    adb_pin_out();
    data_lo();
    adb_delay_us(3000);
    data_hi();
    adb_pin_in();
    busy_wait_ms(10);
}

void AdbHost::fill_status(adb_host_status_t *out) const
{
    if (!out) {
        return;
    }

    out->count = 0;
    out->kbd_configured = 0;
    out->kbd_working = 0;
    out->mouse_configured = 0;
    out->mouse_working = 0;

    for (uint8_t i = 0; i < device_count_ && out->count < ADB_HOST_STATUS_MAX; i++) {
        const DeviceSlot *slot = &devices_[i];
        if (!slot->active) {
            continue;
        }

        adb_host_device_status_t *entry = &out->devices[out->count++];
        entry->addr = slot->addr;
        entry->is_keyboard = (slot->kind == DevKind::Keyboard) ? 1u : 0u;
        entry->working = slot->ever_responded ? 1u : 0u;

        if (slot->kind == DevKind::Keyboard) {
            out->kbd_configured++;
            if (slot->ever_responded) {
                out->kbd_working++;
            }
        } else {
            out->mouse_configured++;
            if (slot->ever_responded) {
                out->mouse_working++;
            }
        }
    }
}

bool AdbHost::has_device_kind(DevKind kind) const
{
    return count_device_kind(kind) > 0;
}

uint8_t AdbHost::count_device_kind(DevKind kind) const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < device_count_; i++) {
        if (devices_[i].active && devices_[i].kind == kind) {
            count++;
        }
    }
    return count;
}

bool AdbHost::has_device_at(uint8_t addr) const
{
    for (uint8_t i = 0; i < device_count_; i++) {
        if (devices_[i].active && devices_[i].addr == addr) {
            return true;
        }
    }
    return false;
}

bool AdbHost::ensure_device(uint8_t addr, DevKind kind)
{
    for (uint8_t i = 0; i < device_count_; i++) {
        if (devices_[i].addr == addr) {
            devices_[i].active = true;
            devices_[i].kind = kind;
            return true;
        }
    }

    if (kind == DevKind::Keyboard && has_device_kind(DevKind::Keyboard)) {
        return false;
    }
    if (kind == DevKind::Pointing && count_device_kind(DevKind::Pointing) >= kMaxPointing) {
        return false;
    }
    if (device_count_ >= kMaxDevices) {
        return false;
    }

    DeviceSlot *slot = &devices_[device_count_++];
    slot->addr = addr;
    slot->kind = kind;
    slot->active = true;
    slot->last_r0 = -1;
    slot->last_err = 0;
    slot->poll_attempts = 0;
    slot->ever_responded = false;

    if (global_debug) {
        Serial.print("ADB host: add ");
        Serial.print(kind == DevKind::Keyboard ? "kbd" : "mouse");
        Serial.print(" @0x");
        Serial.println(addr, HEX);
    }
    return true;
}

void AdbHost::ensure_pointing_candidate(uint8_t addr)
{
    if (!addr_is_pointing_slot(addr) || has_device_at(addr)) {
        return;
    }
    ensure_device(addr, DevKind::Pointing);
}

void AdbHost::log_device_list(const char *label) const
{
    if (!global_debug) {
        return;
    }

    Serial.print("ADB host: ");
    Serial.print(label);
    if (device_count_ == 0) {
        Serial.println(" (none)");
        return;
    }
    for (uint8_t i = 0; i < device_count_; i++) {
        Serial.print(i == 0 ? " " : ", ");
        Serial.print(devices_[i].kind == DevKind::Keyboard ? "kbd" : "mouse");
        Serial.print("@0x");
        Serial.print(devices_[i].addr, HEX);
    }
    Serial.println("");
}

void AdbHost::try_keyboard_default(void)
{
    if (has_device_kind(DevKind::Keyboard)) {
        return;
    }
    // QMK/TMK: default Apple keyboard address is 0x02; Talk R3 often has no payload.
    ensure_device(0x02, DevKind::Keyboard);
}

bool AdbHost::try_mouse_relocation(void)
{
    if (has_device_at(0x0F)) {
        return false;
    }

    // Quadra 700 trackball (adbmon H1): Mac sends Listen @3/0xFFE and @F/0xEFE, then
    // polls Talk R0 @0xF. Retry on rescan so hot-plugged trackballs can be relocated.
    struct {
        uint8_t listen_addr;
        uint16_t payload;
    } steps[] = {
        {0x03, 0x0FFE},
        {0x0F, 0x0EFE},
    };

    for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        if (global_debug) {
            Serial.print("ADB host: Listen R3 @0x");
            Serial.print(steps[i].listen_addr, HEX);
            Serial.print(" data=0x");
            Serial.println(steps[i].payload, HEX);
        }
        if (!listen(steps[i].listen_addr, 3, steps[i].payload)) {
            if (global_debug) {
                Serial.println("ADB host: Listen failed");
            }
            return false;
        }
        busy_wait_ms(5);
    }

    return ensure_device(0x0F, DevKind::Pointing);
}

void AdbHost::try_pointing_defaults(void)
{
    ensure_pointing_candidate(0x03);
}

void AdbHost::discover_pointing_devices(bool allow_relocation)
{
    if (allow_relocation && !has_device_at(0x0F)) {
        if (global_debug) {
            Serial.println("ADB host: try trackball relocation -> @0xF");
        }
        try_mouse_relocation();
    }

    ensure_pointing_candidate(0x03);
    ensure_pointing_candidate(0x0E);
    ensure_pointing_candidate(0x0F);
}

void AdbHost::scan_r3(bool verbose)
{
    for (unsigned i = 0; i < kScanCount; i++) {
        uint8_t addr = kScanAddrs[i];
        int32_t reg3 = talk(addr, 3);
        if (reg3 < 0) {
            if (verbose && global_debug) {
                Serial.print("ADB host: scan @0x");
                Serial.print(addr, HEX);
                Serial.print(" no reply (");
                Serial.print(rx_err_label(reg3));
                if (reg3 == -1 || reg3 == -2 || reg3 == -3 || reg3 == -4) {
                    Serial.print(" t=");
                    Serial.print(last_rx_timing_, DEC);
                }
                Serial.print(") IN=");
                Serial.println(data_in() ? "high" : "low");
            }
            continue;
        }

        uint8_t handler = (uint8_t)(reg3 & 0xFF);
        bool is_keyboard = false;
        if (!classify_r3_valid(addr, handler, &is_keyboard)) {
            continue;
        }

        if (verbose && global_debug) {
            Serial.print("ADB host: scan @0x");
            Serial.print(addr, HEX);
            Serial.print(" R3=0x");
            Serial.print((uint16_t)reg3, HEX);
            Serial.print(" handler=0x");
            Serial.println(handler, HEX);
        }

        ensure_device(addr, is_keyboard ? DevKind::Keyboard : DevKind::Pointing);
    }
}

void AdbHost::enumerate_bus(bool reset_slots)
{
    if (reset_slots) {
        device_count_ = 0;
    }

    if (global_debug) {
        Serial.println("ADB host: enumerate Talk R3 @0x02..0x07,0x0E,0x0F");
    }

    scan_r3(true);
    discover_pointing_devices(true);
    try_keyboard_default();
    log_device_list("poll targets");
}

void AdbHost::rescan_bus(void)
{
    if (global_debug) {
        Serial.println("ADB host: rescan");
    }

    // Hot-plugged ADB devices only appear after a bus reset (same as Mac at power-on).
    if (global_debug) {
        Serial.println("ADB host: global reset for hotplug scan");
    }
    global_reset();
    busy_wait_ms(50);

    scan_r3(false);
    discover_pointing_devices(true);
    try_keyboard_default();
    log_device_list("rescan targets");
}

void AdbHost::log_poll_result(uint8_t addr, uint8_t reg, int32_t result, DevKind kind)
{
    if (!global_debug) {
        return;
    }

    DeviceSlot *slot = nullptr;
    for (uint8_t i = 0; i < device_count_; i++) {
        if (devices_[i].addr == addr && devices_[i].kind == kind) {
            slot = &devices_[i];
            break;
        }
    }
    if (!slot) {
        return;
    }

    if (result < 0) {
        if (result != slot->last_err) {
            Serial.print("ADB host: RX fail Talk R");
            Serial.print(reg, DEC);
            Serial.print(" @0x");
            Serial.print(addr, HEX);
            Serial.print(" (");
            Serial.print(rx_err_label(result));
            Serial.print(" err=");
            Serial.print(result, DEC);
            if (result == -1 || result == -2 || result == -3 || result == -4 || result == -5) {
                Serial.print(" t=");
                Serial.print(last_rx_timing_, DEC);
            }
            Serial.print(") IN=");
            Serial.println(data_in() ? "high" : "low");
            slot->last_err = result;
        }
        return;
    }

    slot->last_err = 0;
    if (result == slot->last_r0) {
        return;
    }
    slot->last_r0 = result;

    Serial.print("ADB host: RX Talk R");
    Serial.print(reg, DEC);
    Serial.print(" @0x");
    Serial.print(addr, HEX);
    Serial.print(" = 0x");
    Serial.println((uint16_t)result, HEX);
}

void AdbHost::maybe_log_summary(void)
{
    if (!global_debug) {
        return;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_summary_ms_ < 2000u) {
        return;
    }
    last_summary_ms_ = now;

    Serial.print("ADB host: status polls=");
    Serial.print(poll_count_, DEC);
    if (device_count_ == 0) {
        Serial.println(" devices=none");
        return;
    }
    for (uint8_t i = 0; i < device_count_; i++) {
        Serial.print(" ");
        Serial.print(devices_[i].kind == DevKind::Keyboard ? "kbd" : "mouse");
        Serial.print("@0x");
        Serial.print(devices_[i].addr, HEX);
        Serial.print(devices_[i].last_err < 0 ? " err=" : " ok");
        if (devices_[i].last_err < 0) {
            Serial.print(devices_[i].last_err, DEC);
        } else if (devices_[i].last_r0 >= 0) {
            Serial.print(" R0=0x");
            Serial.print((uint16_t)devices_[i].last_r0, HEX);
        }
    }
    Serial.println("");
}

void AdbHost::leave_mode(void)
{
    if (entered_ && global_debug) {
        Serial.println("ADB host: leave mode");
    }
    entered_ = false;
    device_count_ = 0;
    poll_count_ = 0;
    last_summary_ms_ = 0;
    last_rescan_ms_ = 0;
}

void AdbHost::on_enter_mode(void)
{
    if (entered_) {
        return;
    }

    if (global_debug) {
        Serial.println("ADB host: enter mode");
        Serial.println("ADB host: global reset (3ms), settle 50ms");
    }

    adb_to_usb_init();
    log_bus_wiring_test();
    global_reset();
    busy_wait_ms(50);
    device_count_ = 0;
    poll_count_ = 0;
    last_summary_ms_ = to_ms_since_boot(get_absolute_time());
    last_rescan_ms_ = last_summary_ms_;

    enumerate_bus(true);
    entered_ = true;
}

void AdbHost::poll(void)
{
    if (!entered_) {
        on_enter_mode();
    }

    poll_count_++;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_rescan_ms_ >= kRescanMs) {
        last_rescan_ms_ = now;
        rescan_bus();
    }

    for (uint8_t i = 0; i < device_count_; i++) {
        DeviceSlot *slot = &devices_[i];
        if (!slot->active) {
            continue;
        }

        int32_t reg0 = talk(slot->addr, 0);
        slot->poll_attempts++;
        log_poll_result(slot->addr, 0, reg0, slot->kind);
        if (reg0 < 0) {
            continue;
        }

        slot->ever_responded = true;

        if (slot->kind == DevKind::Keyboard) {
            adb_to_usb_keyboard_reg0((uint16_t)reg0);
        } else {
            adb_to_usb_mouse_reg0((uint16_t)reg0);
        }
    }

    maybe_log_summary();
}
