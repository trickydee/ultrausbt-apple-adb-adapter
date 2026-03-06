//---------------------------------------------------------------------------
//  HIDHopper ADB
//  This project is based on QuokkaADB:
//
//	QuokkaADB ADB keyboard and mouse adapter
//
//     Copyright 2011 Jun WAKO <wakojun@gmail.com>
//     Copyright 2013 Shay Green <gblargg@gmail.com>
//	   Copyright (C) 2017 bbraun
//     Copyright (C) 2019 Ha Thach (tinyusb.org)
//	   Copyright (C) 2020 difegue
//	   Copyright (C) 2021-2022 akuker
//     Copyright (C) 2022 Rabbit Hole Computing LLC
//
//  This file is part of QuokkaADB.
//
//  This file is free software: you can redistribute it and/or modify it under 
//  the terms of the GNU General Public License as published by the Free 
//  Software Foundation, either version 3 of the License, or (at your option) 
//  any later version.
//
//  This file is distributed in the hope that it will be useful, but WITHOUT ANY 
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS 
//  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more 
//  details.
//
//  You should have received a copy of the GNU General Public License along 
//  with file. If not, see <https://www.gnu.org/licenses/>.
//
//  Portions of this code were originally released under a Modified BSD 
//  License. See LICENSE in the root of this repository for more info.
//  
//  Portions of this code were originally released under the MIT License (MIT). 
//  See LICENSE in the root of this repository for more info.
//----------------------------------------------------------------------------

//#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"

#include "tusb.h"
#include "pico/stdio.h"
#include "rp2040_serial.h"
#include "adb.h"
#include "quokkadb_gpio.h"
#include "adbkbdparser.h"
#include "adbmouseparser.h"
#include "flashsettings.h"
#include "platform_config.h"
#include "bt_hid_bridge.h"
#include <cstdio>
#include "hardware/pio.h"

#if ENABLE_BLUEPAD32
#include "bluepad32_api.h"
#include "bt_pairing_sync.h"
#include "sidewinder_pack.h"
#endif
#include "display/display.h"

using rp2040_serial::Serial;

// Globals
extern uint8_t mousepending;
extern uint8_t kbdpending;
extern uint8_t kbdskip;
extern uint16_t kbdprev0;
extern uint16_t mousereg0;
extern uint16_t kbdreg0;
extern uint16_t kbdreg2;
extern uint8_t kbdsrq;
extern uint8_t mousesrq;
extern uint32_t kbskiptimer;
extern uint16_t modifierkeys;
extern bool adb_reset;
extern uint8_t kbd_addr;
extern uint8_t mouse_addr;
extern volatile bool adb_collision;
#if ENABLE_BLUEPAD32
extern uint8_t game_addr;
extern uint8_t gamepending;
extern uint8_t gamesrq;
extern uint8_t game_joystick_packet[];
#endif
bool usb_reset = false;
bool global_debug = false;

static uint32_t last_adb_cmd_time = 0;
static bool adb_ever_received_cmd = false;  /* display "Connected" only after at least one command */

AdbInterface adb;

ADBKbdRptParser KeyboardPrs;
ADBMouseRptParser MousePrs(KeyboardPrs);
FlashSettings setting_storage;

/*------------- MAIN -------------*/

// core1: handle host events
void core1_main() {
  tuh_init(0);
  led_blink(1);
#if ENABLE_BLUEPAD32
  // Allow Core 0 (btstack) to coordinate flash access during pairing; avoids hangs (see amigahid-pico).
  bt_pairing_sync_core1_init();
#endif
  /*------------ Core1 main loop ------------*/
  while (true) {
#if ENABLE_BLUEPAD32
    // Pause during BT pairing so flash/GATT can complete without contention.
    // Use sleep_ms so Core 1 yields and flash_safe_execute lockout IRQ can run (NVM write on 2nd device).
    if (bt_pairing_sync_is_core1_paused()) {
      sleep_ms(1);
      continue;
    }
#endif
    tuh_task(); // tinyusb host task

    KeyboardPrs.ChangeUSBKeyboardLEDs();
    
    if (true == usb_reset)
    {
      KeyboardPrs.Reset();
      usb_reset = false;
    }
  }
}

// core0: handle device events
int quokkadb(void) {
  // Clock: 225 MHz for Bluetooth builds (matches Atari adapter / logronoid config).
  // CYW43 can have issues at very high speeds (270 MHz causes STALL timeouts); 225 MHz is a stable balance.
#if ENABLE_BLUEPAD32
  const uint32_t clock_khz = 225000;
  if (!set_sys_clock_khz(clock_khz, true)) {
    printf("set_sys_clock_khz(%lu MHz) failed, using default\n", (unsigned long)(clock_khz / 1000));
  }
#else
  set_sys_clock_khz(125000, true);
#endif

  led_gpio_init();
  led_blink(1);
  stdio_init_all();
  uart_gpio_init();
  adb_gpio_init();
  


  
  setting_storage.init();

  display_init();

  //  Block this core when the core1 is writing to flash

  multicore_reset_core1();
  // all USB task run in core1
  multicore_launch_core1(core1_main);
  multicore_lockout_victim_init();
  
  printf("%s\n", PLATFORM_FW_VER_STRING);
  display_show_splash();
  srand(time_us_32());

#if ENABLE_BLUEPAD32
  if (bluepad32_init() == NULL) {
    printf("Bluepad32 init failed (BT disabled)\n");
  }
#endif

/*------------ Core0 main loop ------------*/
  while (true) {
    int16_t cmd = 0;

    /* Update display ADB status: connected, device IDs (G=gamepad when BT gamepad connected), SRQ, collision */
    {
      uint32_t now = time_us_32();
      int connected = (adb_ever_received_cmd && (now - last_adb_cmd_time) < 2000000u) ? 1 : 0;
      int srq = (kbdsrq || mousesrq) ? 1 : 0;
      int collision = adb_collision ? 1 : 0;
#if ENABLE_BLUEPAD32
      if (gamesrq) srq = 1;
      uint8_t game_id = (bluepad32_get_gamepad_count() > 0) ? game_addr : 0;
      display_set_adb_status(connected, kbd_addr, mouse_addr, game_id, srq, collision);
#else
      display_set_adb_status(connected, kbd_addr, mouse_addr, 0, srq, collision);
#endif
    }
    display_handle_buttons();

#if ENABLE_BLUEPAD32
    bluepad32_poll();
    process_bluepad32_devices();
    display_set_bt_counts((uint8_t)bluepad32_get_keyboard_count(),
                         (uint8_t)bluepad32_get_mouse_count(),
                         (uint8_t)bluepad32_get_gamepad_count());
    /* Feed first gamepad to ADB joystick (Sidewinder format) */
    {
      uint8_t gp_buf[BLUEPAD32_GAMEPAD_STORAGE_SIZE];
      if (bluepad32_get_gamepad(0, gp_buf)) {
        sidewinder_pack_from_gamepad(gp_buf, game_joystick_packet);
        gamepending = 1;
      }
    }
#endif

    if (!kbdpending)
    {
      if (KeyboardPrs.PendingKeyboardEvent())
      {
        kbdreg0 = KeyboardPrs.GetAdbRegister0();
        kbdreg2 = KeyboardPrs.GetAdbRegister2();
        kbdpending = 1;

      }
    }
    
    if (!mousepending)
    {
      if (MousePrs.MouseChanged())
      {
        mousereg0 = MousePrs.GetAdbRegister0();
        mousepending = 1;
      }
    }

    led_off();
#if ENABLE_BLUEPAD32
    cmd = adb.ReceiveCommand(mousesrq | kbdsrq | gamesrq);
#else
    cmd = adb.ReceiveCommand(mousesrq | kbdsrq);
#endif
    if(setting_storage.settings()->led_on)
    {
      led_on();
    }  
    adb.ProcessCommand(cmd);
    /* Only refresh "connected" when we actually received a command from the bus (cmd >= 0).
     * When unplugged from ADB, ReceiveCommand returns -1 and we must not update the time,
     * so the 2s timeout can expire and we show "ADB: --". */
    if (cmd >= 0) {
      adb_ever_received_cmd = true;
      last_adb_cmd_time = time_us_32();
    }

    if (adb_reset)
    {
      adb.Reset();
      adb_reset = false;
      usb_reset = true;
      Serial.println("ALL: Resetting devices");
    } 
  }
  return 0;
}