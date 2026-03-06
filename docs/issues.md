# HIDHopper ADB – Issues log

Known issues and limitations. No fix scheduled for the moment unless noted.

---

## 1. Lockups on Pico W (RP2040) since adding gamepad code

**Board:** Pico W (RP2040 with CYW43)

**Summary:** Pairing and/or operation can lock up or hang on the RP2040-based Pico W since gamepad (joystick) support was added. First-attempt pairing sometimes fails (e.g. stops at “Requesting device information” after “Pairing complete, success”); second-device pairing (e.g. keyboard after mouse) has also been seen to hang. Retrying or switching to Pico 2 W (RP2350) often avoids the problem.

**Status:** Deferred. See `docs/bluetooth-pairing-and-memory.md` for timing/config mitigations and comparison with amigahid-pico / Atari.

---

## 2. DS5 disconnects when scanning ADB bus with tattletech (Pico 2 W)

**Board:** Pico 2 W (RP2350 with CYW43)

**Summary:** When the ADB bus is scanned (e.g. using tattletech on Mac), the connected DualSense 5 (DS5) Bluetooth gamepad disconnects. Logs show L2CAP channel close, device disconnection, and “Couldn’t not find hid_device for cid” during teardown. The issue occurs only when the DS5 gamepad is attached; keyboard and mouse do not show this behaviour.

**Likely cause:** The burst of ADB traffic (and possibly display updates) during the scan may starve the BLE stack or contend for the CYW43 bus long enough that the DS5 link is dropped.

**Status:** Deferred.
