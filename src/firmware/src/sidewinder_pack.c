/**
 * Pack Bluepad32 uni_gamepad_t into Microsoft Sidewinder 3D Pro ADB 56-bit packet.
 * See docs/ms-sidewinder-adb-packet.md.
 */

#if ENABLE_BLUEPAD32

#include "sidewinder_pack.h"
#include "controller/uni_gamepad.h"
#include <string.h>

#define AXIS_MIN   (-512)
#define AXIS_MAX   511
#define AXIS_CENTER 0
#define X_MAX  0x3FFu
#define Y_MAX  0x3FFu
#define RUDDER_MAX 0x1FFu
#define THROTTLE_MAX 0x3FFu

/* Sidewinder: 1 = released, 0 = pressed. So set bit when NOT pressed. */
#define RELEASED 1
#define PRESSED  0

static uint16_t clamp_u10(int32_t val, int32_t lo, int32_t hi, uint32_t out_max) {
    if (val <= lo) return 0;
    if (val >= hi) return (uint16_t)out_max;
    /* map [lo, hi] -> [0, out_max] */
    uint32_t range = (uint32_t)(hi - lo);
    return (uint16_t)((uint32_t)(val - lo) * out_max / range);
}

static uint16_t clamp_u9(int32_t val, int32_t lo, int32_t hi) {
    if (val <= lo) return 0;
    if (val >= hi) return RUDDER_MAX;
    uint32_t range = (uint32_t)(hi - lo);
    return (uint16_t)((uint32_t)(val - lo) * RUDDER_MAX / range);
}

/* Hat: 0=off, 1=up, 2=upleft, 3=left, 4=downleft, 5=down, 6=downright, 7=right, 8=upright */
static uint8_t dpad_to_hat(uint8_t dpad) {
    int up    = (dpad & DPAD_UP)    ? 1 : 0;
    int down  = (dpad & DPAD_DOWN)  ? 1 : 0;
    int left  = (dpad & DPAD_LEFT)  ? 1 : 0;
    int right = (dpad & DPAD_RIGHT) ? 1 : 0;
    if (!up && !down && !left && !right) return 0;
    if (up && !down && !left && !right) return 1;
    if (up && !down && left && !right) return 2;
    if (!up && !down && left && !right) return 3;
    if (!up && down && left && !right) return 4;
    if (!up && down && !left && !right) return 5;
    if (!up && down && !left && right) return 6;
    if (!up && !down && !left && right) return 7;
    if (up && !down && !left && right) return 8;
    return 0;
}

void sidewinder_pack_from_gamepad(const void* gamepad, uint8_t* buffer) {
    const uni_gamepad_t* gp = (const uni_gamepad_t*)gamepad;
    memset(buffer, 0, SIDEWINDER_PACKET_SIZE);

    /* Base buttons (4): 1=released 0=pressed. Order: bottom-left, bottom-right, top-right, top-left.
     * Map: X, B, Y, A (flight stick style: A=top-left, B=bottom-right, X=bottom-left, Y=top-right) */
    uint8_t base = 0x0Fu;
    if (gp->buttons & BUTTON_X) base &= ~(1u << 0);  /* bottom-left */
    if (gp->buttons & BUTTON_B) base &= ~(1u << 1);  /* bottom-right */
    if (gp->buttons & BUTTON_Y) base &= ~(1u << 2);  /* top-right */
    if (gp->buttons & BUTTON_A) base &= ~(1u << 3);  /* top-left */

    /* X axis 10-bit: 0=left, 0x3FF=right. axis_x is -512..511 */
    uint16_t x = clamp_u10(gp->axis_x, AXIS_MIN, AXIS_MAX, X_MAX);
    /* Y axis 10-bit: 0=up, 0x3FF=down. axis_y: negative = up on stick */
    uint16_t y = clamp_u10(-gp->axis_y, AXIS_MIN, AXIS_MAX, Y_MAX);

    uint8_t hat = dpad_to_hat(gp->dpad);

    /* Rudder 9-bit: axis_rx or (throttle - brake) scaled to 0..0x1FF, center ~0xFF */
    int32_t rudder_val = gp->axis_rx;
    uint16_t rudder = clamp_u9(rudder_val, AXIS_MIN, AXIS_MAX);

    /* Trigger buttons (4): side bottom, side top, top trigger, main trigger. 1=released 0=pressed.
     * Map: trigger L, shoulder L, shoulder R, trigger R */
    uint8_t triggers = 0x0Fu;
    if (gp->buttons & BUTTON_TRIGGER_L) triggers &= ~(1u << 0);
    if (gp->buttons & BUTTON_SHOULDER_L) triggers &= ~(1u << 1);
    if (gp->buttons & BUTTON_SHOULDER_R) triggers &= ~(1u << 2);
    if (gp->buttons & BUTTON_TRIGGER_R) triggers &= ~(1u << 3);

    /* Throttle 10-bit: 0=up (idle), 0x3FF=down (full). gp->throttle is 0..1023 */
    int32_t t = gp->throttle;
    if (t < 0) t = 0;
    if (t > 1023) t = 1023;
    uint16_t throttle = (uint16_t)((uint32_t)t * THROTTLE_MAX / 1023u);

    /* Pack into 56 bits (7 bytes). LSB first. */
    /* Byte 0: bits 0-7 = base (4) + X bits 6-9 (high 4 of 10) */
    buffer[0] = (uint8_t)((base & 0x0Fu) | (((x >> 6) & 0x0Fu) << 4));
    /* Byte 1: bits 8-15 = X bits 0-5 (low 6) + Y bits 8-9 (high 2) */
    buffer[1] = (uint8_t)(((x & 0x3Fu) << 2) | ((y >> 8) & 3u));
    /* Byte 2: bits 16-23 = Y low 8 bits */
    buffer[2] = (uint8_t)(y & 0xFFu);
    /* Byte 3: bits 24-31 = hat (4) + 000 (3) + rudder high 1 bit */
    buffer[3] = (uint8_t)((hat & 0x0Fu) | ((rudder >> 8) << 4));
    /* Byte 4: bits 32-39 = rudder low 8 bits */
    buffer[4] = (uint8_t)(rudder & 0xFFu);
    /* Byte 5: bits 40-47 = triggers (4) + 00 (2) + throttle high 2 bits */
    buffer[5] = (uint8_t)((triggers & 0x0Fu) | ((throttle >> 8) << 4));
    /* Byte 6: bits 48-55 = throttle low 8 bits */
    buffer[6] = (uint8_t)(throttle & 0xFFu);
}

#endif /* ENABLE_BLUEPAD32 */
