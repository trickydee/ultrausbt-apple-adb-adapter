# LED support (firmware)

Status LED behaviour on **GPIO 25** (external D1 on the adapter PCB).

## Board status LED (GPIO 25)

| Item | Location |
|------|----------|
| Pin define | `src/firmware/lib/QuokkADB/include/quokkadb_gpio.h` — `LED_GPIO` (25) |
| Init + blink helper | `src/firmware/lib/QuokkADB/src/quokkadb_gpio.cpp` — `led_gpio_init()`, `led_blink()` |
| Boot / main loop / ADB activity | `src/firmware/lib/QuokkADB/src/quokkadb.cpp` |
| USB mount / unmount blink codes | `src/firmware/lib/QuokkADB/src/usbhost.cpp` |
| User toggle for “busy” LED (saved in flash) | `src/firmware/lib/QuokkADB/src/platformkbdparser.cpp`, `flashsettings` |

### Blink sequence

| Event | Firmware behaviour |
|-------|--------------------|
| One blink on boot | `led_blink(1)` after `led_gpio_init()` on core0 (`quokkadb()`) |
| One blink when USB host is up | `led_blink(1)` after `tuh_init(0)` on core1 (`core1_main()`) |
| Two blinks — keyboard connected (USB) | `led_blink(2)` when a HID boot keyboard mounts (`tuh_hid_mount_cb`) |
| Three blinks — mouse connected (USB) | `led_blink(3)` when a HID boot mouse mounts |
| One blink — device unmounted | `led_blink(1)` in `tuh_hid_umount_cb` |
| Host mode enter | Two blinks (`adb_mode_usb_sync`) |
| Device mode enter | One blink (`adb_mode_usb_sync`) |

### ADB activity indication

The main loop turns the LED **off** before blocking in `adb.ReceiveCommand()`, then **on** (if enabled in settings) after the command returns. The **`L` ghost-type** shortcut (`Ctrl + Shift + Caps Lock + L`) toggles whether that LED tracks ADB activity and persists across power cycles.

GPIO 25 is driven **digitally** (on/off) only — no PWM dimming.

### Pico W / Pico 2 W (CYW43)

Wireless boards also have a **separate** CYW43 LED (SDK “wireless” activity). That is **not** the GPIO 25 status LED.

### Keyboard lock LEDs (Caps / Num / Scroll)

Host keyboard LED state is handled via ADB register 2 / USB OUT reports (`adbkbdparser`, `ChangeUSBKeyboardLEDs`). Independent of the board status LED.

**Known lineage bug:** repeatedly toggling Caps/Num/Scroll can stop the **keyboard’s own** lock LEDs from updating while typing still works.

## See also

- [`hardware.md`](hardware.md) — board GPIO map  
- [`troubleshooting.md`](troubleshooting.md)  
- [`iigs-debugging.md`](iigs-debugging.md)  
