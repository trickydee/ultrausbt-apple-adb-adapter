//---------------------------------------------------------------------------
//  BT-USB-ADB-Adapter
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
#include "pico/flash.h"
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
#include "bt_host_coop.h"
#endif
#include "display/display.h"
extern "C" {
#include "usb_device_map.h"
}

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
bool usb_reset = false;
#ifdef ADB_DEBUG
bool global_debug = true;   // ADB timing/command messages on UART (build with -DADB_DEBUG=ON)
#else
bool global_debug = false;
#endif

static uint32_t last_adb_cmd_time = 0;
static bool adb_ever_received_cmd = false;  /* display "Connected" only after at least one command */

AdbInterface adb;

ADBKbdRptParser KeyboardPrs;
ADBMouseRptParser MousePrs(KeyboardPrs);
FlashSettings setting_storage;

/*------------- MAIN -------------*/

// core1: handle host events
void core1_main() {
  /* Lets Core 0 run BTstack TLV / flash work without XIP conflicts (see amigahid-pico quad_mouse + flash_safe_execute). */
  flash_safe_execute_core_init();
  tuh_init(0);
  led_blink(1);
  /*------------ Core1 main loop ------------*/
  while (true) {
#if ENABLE_BLUEPAD32
    if (bt_host_coop_usb_host_is_paused()) {
      busy_wait_us(5000);
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
  set_sys_clock_khz(125000, true);
  
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

    /* Update display ADB status: connected, device IDs, SRQ (kbd or mouse pending), collision */
    {
      uint32_t now = time_us_32();
      int connected = (adb_ever_received_cmd && (now - last_adb_cmd_time) < 2000000u) ? 1 : 0;
      int srq = (kbdsrq || ((mousesrq && !ADB_IIGS_MOUSE_SUPPRESS_SRQ) ? 1 : 0)) ? 1 : 0;
      int collision = adb_collision ? 1 : 0;
      display_set_adb_status(connected, kbd_addr, mouse_addr, 0, srq, collision);
    }
    display_handle_buttons();

    {
      uint8_t ukb = 0;
      uint8_t um = 0;
      uint8_t ujoy = 0;
      usb_map_get_counts(&ukb, &um, &ujoy);
      display_set_usb_counts(ukb, um, ujoy);
    }

#if ENABLE_BLUEPAD32
    bluepad32_poll();
    process_bluepad32_devices();
    display_set_bt_counts((uint8_t)bluepad32_get_keyboard_count(),
                         (uint8_t)bluepad32_get_mouse_count(),
                         (uint8_t)bluepad32_get_gamepad_count());
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
    // Optionally suppress mouse SRQ extension so the host doesn't spend extra time
    // servicing mouse service requests on some IIgs apps.
#if ADB_IIGS_MOUSE_SUPPRESS_SRQ
    cmd = adb.ReceiveCommand(kbdsrq);
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