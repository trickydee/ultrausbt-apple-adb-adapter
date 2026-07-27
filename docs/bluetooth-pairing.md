# Bluetooth pairing guide

Quick reference for pairing **keyboard**, **mouse**, and **gamepad** (Xbox / Stadia) on **Pico W** and **Pico 2 W** builds. Developer details: [`BT_PAIRING_APPLE_ADB.md`](BT_PAIRING_APPLE_ADB.md).

---

## Which firmware to use

| Image | When |
|-------|------|
| **`dist/BT-USB-ADB-Adapter-firmware-pico2_w-host.uf2`** | Normal use and **pairing tests** (release build) |
| **`dist/...-host-debug.uf2`** | UART troubleshooting only — debug logging changes timing and can mask pairing bugs |

Build both with `./build-all.sh` from the project root.

---

## Recommended pair order

Pair devices **before** or **while** the Mac is booting, in this order:

1. **Mouse**
2. **Keyboard**
3. **Gamepad** (Xbox Wireless BLE or Stadia)

This order is especially important when the **Mac and adapter power on together** (cold boot). Pairing the gamepad first often worked on older firmware but could leave the mouse dead on ADB or flood the keyboard queue.

**PS5 (DualSense)** uses a different radio path and is usually easier — the strict order matters most for **Xbox BLE** and **Stadia**.

---

## Clear stale pairings

On the OLED **Map Devices** screen, hold **`˄+˯`** for **5 seconds** to wipe Bluetooth link keys. Then re-pair in the recommended order.

---

## Mode: ADB → Mac only

Bluetooth pairing runs in **ADB → Mac** (device mode). In **ADB → USB** (host mode), the adapter does not bridge Bluetooth to the PC — switch back on the OLED before pairing. See [`adb-host-mode.md`](adb-host-mode.md).

---

## Common problems

| Symptom | Likely cause | What to do |
|---------|--------------|------------|
| Mouse dead; keyboard OK; UART shows all BT “ready” | Mac **global ADB reset** during pairing (cold boot) | Use firmware **2.2.1+**; pair mouse first; or reset adapter after Mac has booted |
| `unable to enqueue new KeyDown` (debug UART) | Gamepad paired before keyboard | Clear pairings; pair keyboard before gamepad; use **2.2.1+** |
| Xbox won’t reconnect after sleep | Stuck Core 1 pause after failed bond | Clear pairings (`˄+˯` 5 s); power-cycle controller; flash **2.2.1+** |
| `Identity resolving failed` (Logitech) | Often **non-fatal** | Continue if device reaches “device ready” and works |

Full symptom → fix notes: [`troubleshooting.md`](troubleshooting.md) § Bluetooth.

---

## What’s in firmware 2.2.1

Shipped in firmware **2.2.1**:

- Atari/Amiga BLE gamepad pairing fix (Core 1 pause during BTstack flash writes)
- Always-merge keyboard + gamepad input before parsing
- Defer Mac global ADB reset while Bluetooth links are forming

---

## Related docs

- [`troubleshooting.md`](troubleshooting.md) — BT mouse GPIO, host mode, multi-device pairing
- [`gamepad-support.md`](gamepad-support.md) — gamepad button/stick mapping (Phase B)
- [`BT_PAIRING_HANDOFF.md`](BT_PAIRING_HANDOFF.md) — canonical developer recipe (Atari v22.1.0)
- [`FUTURE_WORK.md`](FUTURE_WORK.md) §1 — pairing stability status
