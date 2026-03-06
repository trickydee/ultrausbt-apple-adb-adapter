/**
 * bluepad32 custom platform for HIDHopper ADB (keyboard + mouse only)
 */

#if ENABLE_BLUEPAD32

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <uni.h>

#include "sdkconfig.h"
#include "bt_pairing_sync.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Must use CONFIG_BLUEPAD32_PLATFORM_CUSTOM"
#endif

#define MAX_BT_KEYBOARDS 2
#define MAX_BT_MICE 2
#define MAX_BT_GAMEPADS 1

typedef struct {
    uni_keyboard_t keyboard;
    bool connected;
    bool updated;
    char name[32];
} bt_keyboard_storage_t;

typedef struct {
    uni_mouse_t mouse;
    bool connected;
    bool updated;
    char name[32];
} bt_mouse_storage_t;

typedef struct {
    uni_gamepad_t gamepad;
    bool connected;
    bool updated;
    char name[32];
} bt_gamepad_storage_t;

static bt_keyboard_storage_t bt_keyboards[MAX_BT_KEYBOARDS] = {0};
static bt_mouse_storage_t bt_mice[MAX_BT_MICE] = {0};
static bt_gamepad_storage_t bt_gamepads[MAX_BT_GAMEPADS] = {0};
static uni_hid_device_t* keyboard_device_map[MAX_BT_KEYBOARDS] = {0};
static uni_hid_device_t* mouse_device_map[MAX_BT_MICE] = {0};
static uni_hid_device_t* gamepad_device_map[MAX_BT_GAMEPADS] = {0};

#define MAX_PENDING_NAMES_BY_ADDR 8
typedef struct {
    bd_addr_t addr;
    char name[32];
    bool valid;
} pending_name_by_addr_t;
static pending_name_by_addr_t pending_names_by_addr[MAX_PENDING_NAMES_BY_ADDR] = {0};

static void store_pending_name_by_addr(bd_addr_t addr, const char* name) {
    if (!name || name[0] == '\0') return;
    for (int i = 0; i < MAX_PENDING_NAMES_BY_ADDR; i++) {
        if (!pending_names_by_addr[i].valid ||
            memcmp(pending_names_by_addr[i].addr, addr, 6) == 0) {
            memcpy(pending_names_by_addr[i].addr, addr, 6);
            strncpy(pending_names_by_addr[i].name, name, sizeof(pending_names_by_addr[i].name) - 1);
            pending_names_by_addr[i].name[sizeof(pending_names_by_addr[i].name) - 1] = '\0';
            pending_names_by_addr[i].valid = true;
            return;
        }
    }
}

static const char* get_pending_name_by_addr(bd_addr_t addr) {
    for (int i = 0; i < MAX_PENDING_NAMES_BY_ADDR; i++) {
        if (pending_names_by_addr[i].valid &&
            memcmp(pending_names_by_addr[i].addr, addr, 6) == 0) {
            return pending_names_by_addr[i].name;
        }
    }
    return NULL;
}

static void clear_pending_name_by_addr(bd_addr_t addr) {
    for (int i = 0; i < MAX_PENDING_NAMES_BY_ADDR; i++) {
        if (pending_names_by_addr[i].valid &&
            memcmp(pending_names_by_addr[i].addr, addr, 6) == 0) {
            pending_names_by_addr[i].valid = false;
            return;
        }
    }
}

static int find_slot(uni_hid_device_t* d, uni_hid_device_t** device_map, int max_slots) {
    for (int i = 0; i < max_slots; i++) {
        if (device_map[i] == d) return i;
    }
    for (int i = 0; i < max_slots; i++) {
        if (device_map[i] == NULL) {
            device_map[i] = d;
            return i;
        }
    }
    return -1;
}

static void clear_slot(uni_hid_device_t* d, uni_hid_device_t** device_map, int max_slots) {
    for (int i = 0; i < max_slots; i++) {
        if (device_map[i] == d) {
            device_map[i] = NULL;
            break;
        }
    }
}

static bt_keyboard_storage_t* get_keyboard_storage(uni_hid_device_t* d) {
    int idx = find_slot(d, keyboard_device_map, MAX_BT_KEYBOARDS);
    return idx >= 0 ? &bt_keyboards[idx] : NULL;
}

static bt_mouse_storage_t* get_mouse_storage(uni_hid_device_t* d) {
    int idx = find_slot(d, mouse_device_map, MAX_BT_MICE);
    return idx >= 0 ? &bt_mice[idx] : NULL;
}

static bt_gamepad_storage_t* get_gamepad_storage(uni_hid_device_t* d) {
    int idx = find_slot(d, gamepad_device_map, MAX_BT_GAMEPADS);
    return idx >= 0 ? &bt_gamepads[idx] : NULL;
}

static void my_platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    logi("bluepad32_platform: init\n");
}

static void my_platform_on_init_complete(void) {
    logi("bluepad32_platform: on_init_complete\n");
    sleep_ms(2000);
    logi("Starting Bluetooth scanning...\n");
    uni_bt_start_scanning_and_autoconnect_unsafe();
#if defined(CYW43_WL_GPIO_LED_PIN)
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
#endif
}

static uni_error_t my_platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    ARG_UNUSED(rssi);
    /* Pause Core 1 before connection to avoid flash/display contention during pairing (amigahid-pico). */
    if ((cod & 0x1F00) == 0x0500) { /* HID (keyboard 0x540, mouse 0x5c0, gamepad, etc.) */
        bt_pairing_sync_pause_core1();
        sleep_ms(50);  /* Let Core 1 enter paused loop before connection/NVM (reduces first-attempt GATT hang). */
    }
    if (name && name[0] != '\0') {
        store_pending_name_by_addr(addr, name);
    }
    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_device_connected(uni_hid_device_t* d) {
    ARG_UNUSED(d);
    logi("bluepad32_platform: device connected\n");
    bt_pairing_sync_pause_core1();
    sleep_ms(50);  /* Let Core 1 enter paused loop before GATT/NVM (reduces first-attempt hang). */
}

static void my_platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("bluepad32_platform: device disconnected\n");
    bt_pairing_sync_resume_core1();

    bt_keyboard_storage_t* kb_storage = get_keyboard_storage(d);
    if (kb_storage && kb_storage->connected) {
        kb_storage->connected = false;
        kb_storage->updated = false;
        memset(&kb_storage->keyboard, 0, sizeof(kb_storage->keyboard));
        kb_storage->name[0] = '\0';
        clear_slot(d, keyboard_device_map, MAX_BT_KEYBOARDS);
    }

    bt_mouse_storage_t* mouse_storage = get_mouse_storage(d);
    if (mouse_storage && mouse_storage->connected) {
        mouse_storage->connected = false;
        mouse_storage->updated = false;
        memset(&mouse_storage->mouse, 0, sizeof(mouse_storage->mouse));
        mouse_storage->name[0] = '\0';
        clear_slot(d, mouse_device_map, MAX_BT_MICE);
    }

    bt_gamepad_storage_t* gamepad_storage = get_gamepad_storage(d);
    if (gamepad_storage && gamepad_storage->connected) {
        gamepad_storage->connected = false;
        gamepad_storage->updated = false;
        memset(&gamepad_storage->gamepad, 0, sizeof(gamepad_storage->gamepad));
        gamepad_storage->name[0] = '\0';
        clear_slot(d, gamepad_device_map, MAX_BT_GAMEPADS);
    }
}

static uni_error_t my_platform_on_device_ready(uni_hid_device_t* d) {
    logi("bluepad32_platform: device ready\n");
    /* Longer delay before resuming Core 1 to avoid second-device pairing hangs.
     * Amigahid-pico uses 50ms; with keyboard+mouse+gamepad we use 200ms so NVM
     * and SM can finish and Core 1 display/flash work doesn't overlap next pairing. */
    sleep_ms(200);
    bt_pairing_sync_resume_core1();

    bd_addr_t addr;
    uni_bt_conn_get_address(&d->conn, addr);
    const char* stored_name = get_pending_name_by_addr(addr);
    const char* device_name = NULL;
    if (uni_hid_device_has_name(d) && d->name[0] != '\0') {
        device_name = d->name;
    } else if (stored_name && stored_name[0] != '\0') {
        device_name = stored_name;
    }

    if (uni_hid_device_is_keyboard(d)) {
        bt_keyboard_storage_t* storage = get_keyboard_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            if (device_name && device_name[0] != '\0') {
                snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), device_name);
                clear_pending_name_by_addr(addr);
            } else {
                snprintf(storage->name, sizeof(storage->name), "Keyboard");
            }
        }
        logi("bluepad32_platform: keyboard ready\n");
    } else if (uni_hid_device_is_mouse(d)) {
        bt_mouse_storage_t* storage = get_mouse_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            if (device_name && device_name[0] != '\0') {
                snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), device_name);
                clear_pending_name_by_addr(addr);
            } else {
                snprintf(storage->name, sizeof(storage->name), "Mouse");
            }
        }
        logi("bluepad32_platform: mouse ready\n");
    } else if (uni_hid_device_is_gamepad(d)) {
        bt_gamepad_storage_t* storage = get_gamepad_storage(d);
        if (storage) {
            storage->connected = true;
            storage->updated = false;
            if (device_name && device_name[0] != '\0') {
                snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), device_name);
                clear_pending_name_by_addr(addr);
            } else {
                snprintf(storage->name, sizeof(storage->name), "Gamepad");
            }
        }
        logi("bluepad32_platform: gamepad ready\n");
    } else {
        logi("bluepad32_platform: unsupported device type (keyboard/mouse/gamepad)\n");
    }

    return UNI_ERROR_SUCCESS;
}

static void my_platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    switch (ctl->klass) {
        case UNI_CONTROLLER_CLASS_KEYBOARD: {
            bt_keyboard_storage_t* storage = get_keyboard_storage(d);
            if (storage) {
                if (!storage->connected) {
                    storage->connected = true;
                    if (storage->name[0] == '\0') {
                        if (uni_hid_device_has_name(d) && d->name[0] != '\0') {
                            snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), d->name);
                        } else {
                            snprintf(storage->name, sizeof(storage->name), "Keyboard");
                        }
                    }
                }
                storage->keyboard = ctl->keyboard;
                storage->updated = true;
            }
            break;
        }
        case UNI_CONTROLLER_CLASS_MOUSE: {
            bt_mouse_storage_t* storage = get_mouse_storage(d);
            if (storage) {
                if (!storage->connected) {
                    storage->connected = true;
                    if (storage->name[0] == '\0') {
                        if (uni_hid_device_has_name(d) && d->name[0] != '\0') {
                            snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), d->name);
                        } else {
                            snprintf(storage->name, sizeof(storage->name), "Mouse");
                        }
                    }
                }
                storage->mouse = ctl->mouse;
                storage->updated = true;
            }
            break;
        }
        case UNI_CONTROLLER_CLASS_GAMEPAD: {
            bt_gamepad_storage_t* storage = get_gamepad_storage(d);
            if (storage) {
                if (!storage->connected) {
                    storage->connected = true;
                    if (storage->name[0] == '\0') {
                        if (uni_hid_device_has_name(d) && d->name[0] != '\0') {
                            snprintf(storage->name, sizeof(storage->name), "%.*s", (int)(sizeof(storage->name) - 1), d->name);
                        } else {
                            snprintf(storage->name, sizeof(storage->name), "Gamepad");
                        }
                    }
                }
                storage->gamepad = ctl->gamepad;
                storage->updated = true;
            }
            break;
        }
        default:
            break;
    }
}

static const uni_property_t* my_platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void my_platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    ARG_UNUSED(data);
    if (event == UNI_PLATFORM_OOB_BLUETOOTH_ENABLED) {
        logi("bluepad32_platform: Bluetooth enabled\n");
    }
}

struct uni_platform* get_my_platform(void) {
    static struct uni_platform plat = {
        .name = "HIDHopper ADB",
        .init = my_platform_init,
        .on_init_complete = my_platform_on_init_complete,
        .on_device_discovered = my_platform_on_device_discovered,
        .on_device_connected = my_platform_on_device_connected,
        .on_device_disconnected = my_platform_on_device_disconnected,
        .on_device_ready = my_platform_on_device_ready,
        .on_oob_event = my_platform_on_oob_event,
        .on_controller_data = my_platform_on_controller_data,
        .get_property = my_platform_get_property,
    };
    return &plat;
}

bool bluepad32_get_keyboard(int idx, void* out_keyboard) {
    if (idx < 0 || idx >= MAX_BT_KEYBOARDS || !out_keyboard) return false;
    if (bt_keyboards[idx].connected && bt_keyboards[idx].updated) {
        *(uni_keyboard_t*)out_keyboard = bt_keyboards[idx].keyboard;
        bt_keyboards[idx].updated = false;
        return true;
    }
    return false;
}

int bluepad32_get_keyboard_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_BT_KEYBOARDS; i++) {
        if (bt_keyboards[i].connected) n++;
    }
    return n;
}

bool bluepad32_get_mouse(int idx, void* out_mouse) {
    if (idx < 0 || idx >= MAX_BT_MICE || !out_mouse) return false;
    if (bt_mice[idx].connected && bt_mice[idx].updated) {
        *(uni_mouse_t*)out_mouse = bt_mice[idx].mouse;
        bt_mice[idx].updated = false;
        return true;
    }
    return false;
}

int bluepad32_get_mouse_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_BT_MICE; i++) {
        if (bt_mice[i].connected) n++;
    }
    return n;
}

bool bluepad32_get_gamepad(int idx, void* out_gamepad) {
    if (idx < 0 || idx >= MAX_BT_GAMEPADS || !out_gamepad) return false;
    if (bt_gamepads[idx].connected && bt_gamepads[idx].updated) {
        *(uni_gamepad_t*)out_gamepad = bt_gamepads[idx].gamepad;
        bt_gamepads[idx].updated = false;
        return true;
    }
    return false;
}

int bluepad32_get_gamepad_count(void) {
    int n = 0;
    for (int i = 0; i < MAX_BT_GAMEPADS; i++) {
        if (bt_gamepads[i].connected) n++;
    }
    return n;
}

void bluepad32_delete_pairing_keys(void) {
    uni_bt_del_keys_unsafe();
}

const char* bluepad32_get_device_name(char device_type, int idx) {
    if (idx < 0 || idx >= 2) return NULL;
    if (device_type == 'K' && idx < MAX_BT_KEYBOARDS && bt_keyboards[idx].connected) {
        return bt_keyboards[idx].name;
    }
    if (device_type == 'M' && idx < MAX_BT_MICE && bt_mice[idx].connected) {
        return bt_mice[idx].name;
    }
    if (device_type == 'G' && idx < MAX_BT_GAMEPADS && bt_gamepads[idx].connected) {
        return bt_gamepads[idx].name;
    }
    return NULL;
}

#endif // ENABLE_BLUEPAD32
