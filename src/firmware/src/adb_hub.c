/**
 * ADB passthrough hub — host-driven enumeration helpers.
 */

#include "adb_hub.h"

#include <stdlib.h>

#define KBD_DEFAULT_ADDR 0x02
#define MOUSE_DEFAULT_ADDR 0x03

extern uint8_t kbd_addr;
extern uint8_t mouse_addr;

static bool s_hub_mode;
static bool s_kbd_host_assigned;
static bool s_mouse_host_assigned;
static bool s_mouse_r3_collision;

void adb_hub_configure(bool hub_mode)
{
    s_hub_mode = hub_mode;
}

void adb_hub_restore_addresses(void)
{
    kbd_addr = KBD_DEFAULT_ADDR;
    mouse_addr = MOUSE_DEFAULT_ADDR;
    adb_hub_clear_host_assigned();
}

bool adb_hub_is_enabled(void)
{
    return s_hub_mode;
}

uint8_t adb_hub_effective_kbd_addr(void)
{
    return kbd_addr;
}

uint8_t adb_hub_effective_mouse_addr(void)
{
    return mouse_addr;
}

bool adb_hub_is_local_address(uint8_t addr)
{
    return addr == kbd_addr || addr == mouse_addr;
}

void adb_hub_clear_host_assigned(void)
{
    s_kbd_host_assigned = false;
    s_mouse_host_assigned = false;
    s_mouse_r3_collision = false;
}

bool adb_hub_is_host_assigned(bool is_mouse)
{
    return is_mouse ? s_mouse_host_assigned : s_kbd_host_assigned;
}

void adb_hub_on_host_address_assigned(bool is_mouse)
{
    if (is_mouse) {
        s_mouse_host_assigned = true;
    } else {
        s_kbd_host_assigned = true;
    }
}

void adb_hub_note_reg3_collision(bool is_mouse)
{
    if (is_mouse) {
        s_mouse_r3_collision = true;
    }
}

uint8_t adb_hub_propose_reg3_address(uint8_t current_addr, bool is_mouse)
{
    /* After a register-3 collision at default mouse address, prefer 0x04–0x07
     * so a chained trackball/joystick can keep 0x03. */
    if (s_hub_mode && is_mouse && current_addr == MOUSE_DEFAULT_ADDR && s_mouse_r3_collision) {
        static const uint8_t slots[] = {0x04, 0x05, 0x06, 0x07};
        return slots[rand() % 4];
    }
    /* Space Aliens: propose a random address in bits 11–8 during enumeration. */
    return (uint8_t)(rand() & 0x0F);
}
