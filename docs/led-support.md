# LED support (firmware)

This document describes **status LED** behaviour on GPIO 15 and where it is implemented in code.

## Board status LED (GPIO 15)

The primary **status LED** is driven on **GPIO 25** (external D1 on the adapter PCB).

| Item | Location |
|------|-----------|
| Pin define | `src/firmware/lib/QuokkADB/include/quokkadb_gpio.h` — `LED_GPIO` (25) |
| Init + blink helper | `src/firmware/lib/QuokkADB/src/quokkadb_gpio.cpp` — `led_gpio_init()`, `led_blink()` |
| Boot / main loop / ADB activity | `src/firmware/lib/QuokkADB/src/quokkadb.cpp` |
| USB mount / unmount blink codes | `src/firmware/lib/QuokkADB/src/usbhost.cpp` |
| User toggle for “busy” LED (saved in flash) | `src/firmware/lib/QuokkADB/src/platformkbdparser.cpp`, `flashsettings` |

### Blink sequence

| Event | Firmware behaviour |
|---------------|-------------------|
| One blink on boot | `led_blink(1)` after `led_gpio_init()` on core0 (`quokkadb()`) |
| One blink when USB host is up | `led_blink(1)` after `tuh_init(0)` on core1 (`core1_main()`) |
| Two blinks — keyboard connected (USB) | `led_blink(2)` when a HID boot keyboard interface mounts (`tuh_hid_mount_cb` in `usbhost.cpp`) |
| Three blinks — mouse connected (USB) | `led_blink(3)` when a HID boot mouse interface mounts (same) |
| One blink — device unmounted | `led_blink(1)` in `tuh_hid_umount_cb` (`usbhost.cpp`) |

### ADB activity indication

During normal operation, the main loop turns the LED **off** before blocking in `adb.ReceiveCommand()`, then turns it **on** (if enabled in settings) after the command returns—so activity is tied to the configured “ADB busy” LED option (`led_on` in flash). The **`L` ghost-type** shortcut (`Ctrl + Shift + Caps Lock + L`) toggles whether that LED tracks ADB activity and persists across power cycles.

### “Very dim when idle”

Documentation sometimes refers to a **very dim** idle state. In this firmware, GPIO 15 is driven **digitally** (on/off) only; there is **no PWM dimming** implemented.

### Pico W / Pico 2 W (CYW43)

Wireless boards also have a **separate** LED associated with the **CYW43** module (commonly wired so the SDK uses **GPIO 0** for “wireless” activity). That is **not** the same as the **GPIO 15** status LED above.

### Keyboard lock LEDs (Caps / Num / Scroll)

**Host keyboard LED state** (Caps Lock, Num Lock, Scroll Lock indicators on the physical USB keyboard) is handled via ADB register 2 / USB OUT reports (`adbkbdparser`, `ChangeUSBKeyboardLEDs`). That is **independent** of the **GPIO 15** status LED.

---

## Known bug (keyboard lock LEDs)

From prior QuokkADB lineage — **Known bug**:

Repeatedly pressing keys that toggle LED states (Caps Lock, Num Lock, Scroll Lock) can cause the **keyboard’s own lock LEDs** to **stop updating** after many toggles. **Typing and lock behavior (e.g. Caps Lock) can still work**; only the **visual LED indicators** on the keyboard may no longer change.

This is separate from GPIO 15 status blinks and from the CYW43 LED.

---

## See also

- [`hardware.md`](hardware.md) — board GPIO map
- [`iigs-debugging.md`](iigs-debugging.md) — IIgs-focused troubleshooting (if present in tree)
