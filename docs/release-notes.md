# HIDHopper ADB – Release notes

## 1.0.2

- **Right mouse button (ctrl-click) no longer hangs the device.**  
  When the right button was used in ctrl-click mode (default for compatibility with System 6 / System 7), the firmware could deadlock waiting for the keyboard queue to drain from inside the mouse report handler. The blocking waits were removed; Ctrl and click events are enqueued and sent by the main loop, so ctrl-click behaviour is unchanged and the device no longer hangs.

## 1.0.1

- Firmware version numbering: single source of truth in `CMakeLists.txt`; banner and identity show “HIDHopper ADB” and version.
- Flash settings sector moved to avoid overlap with BTstack TLV region on wireless builds.
