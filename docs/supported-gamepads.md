# Supported gamepads (ADB joystick)

On **Pico W** and **Pico 2 W** builds, the ADB joystick (Sidewinder 3D Pro format at address 0x04) is driven by the first connected Bluetooth gamepad. The firmware uses [Bluepad32](https://github.com/ricardoquesada/bluepad32), which supports many controllers. All of them are exposed as the same virtual gamepad and mapped into the Sidewinder packet (axes, hat, buttons, throttle, rudder).

## Modern gamepads (recommended)

These work out of the box and are well-tested with Bluepad32:

| Controller | Protocol | Notes |
|------------|----------|--------|
| **Sony DualSense (PS5)** | BR/EDR | Full support: axes, d-pad, face buttons, shoulders, triggers, thumb sticks. |
| **Sony DualShock 4 (PS4)** | BR/EDR | Same as above. |
| **Google Stadia** | BLE | **Requires [Stadia Bluetooth firmware update][stadia-ble]** before it can pair over BLE. After that, full gamepad support. |
| **Nintendo Switch Pro** | BR/EDR | Full support. |
| **Xbox Wireless** | BR/EDR or BLE | Depends on controller firmware; see Bluepad32 docs. |

[stadia-ble]: https://support.google.com/stadia/answer/12475512

## Button mapping (virtual gamepad → Sidewinder)

Bluepad32 uses a common “Xbox-style” virtual layout:

- **Face:** A (South), B (East), X (West), Y (North)  
  → Sidewinder base buttons: X, B, Y, A (bottom-left, bottom-right, top-right, top-left).
- **PlayStation (DualSense / DualShock):** Cross → A, Circle → B, Square → X, Triangle → Y.
- **Stadia:** A → A, B → B, X → X, Y → Y (same physical layout as Xbox).

Left stick → X/Y axes; right stick X → rudder; triggers → trigger buttons; L2/R2 and throttle → throttle axis. See `docs/adb-joystick-bluepad32-mapping.md` and `docs/ms-sidewinder-adb-packet.md` for the full bit layout.

## Other controllers

Bluepad32 also supports DualShock 3, 8BitDo, Steam Controller (with BLE firmware), Wii controllers, and others. They all feed the same virtual gamepad and thus work as the ADB joystick. For the full list and protocol notes, see [Bluepad32 supported gamepads](https://bluepad32.readthedocs.io/en/latest/supported_gamepads/).

## Reference: Amiga adapter (MightyMiggy)

The [MightyMiggy](https://bluepad32.readthedocs.io/en/latest/plat_mightymiggy/) platform is Bluepad32’s Amiga/CD32 adapter. It uses the same Bluepad32 parsers (including DualSense and Stadia via Android/PS5 paths). HIDHopper ADB does not use MightyMiggy’s platform; it uses a minimal platform that only stores keyboard, mouse, and gamepad state. The **gamepad data (including DS5 and Stadia) is already in the same `uni_gamepad_t` format**, so no extra “implementation” of DS5 or Stadia is required in this project—only documentation and testing.
