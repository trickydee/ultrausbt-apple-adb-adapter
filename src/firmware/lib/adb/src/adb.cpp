//---------------------------------------------------------------------------
//  ultrausbt-Apple-ADB-adapter
//  This project is based on ADBuino and QuokkaADB:
//
//	ADBuino & QuokkaADB ADB keyboard and mouse adapter
//
//     Copyright 2011 Jun WAKO <wakojun@gmail.com>
//     Copyright 2013 Shay Green <gblargg@gmail.com>
//	   Copyright (C) 2017 bbraun
//	   Copyright (C) 2020 difegue
//	   Copyright (C) 2021-2022 akuker
//     Copyright (C) 2022 Rabbit Hole Computing LLC
//
//  This file is part of the ADBuino and the QuokkaADB projects.
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
//  with the file. If not, see <https://www.gnu.org/licenses/>.
//
//  Portions of this code were originally released under a Modified BSD 
//  License. See LICENSE in the root of this repository for more info.
//
//----------------------------------------------------------------------------

#include "adb.h"
#include "adb_hub.h"
#include "bithacks.h"
#include "math.h"

#if QUOKKADB
#include "rp2040_serial.h"
using rp2040_serial::Serial;
#endif

uint8_t mouse_addr = MOUSE_DEFAULT_ADDR;
uint8_t kbd_addr = KBD_DEFAULT_ADDR;
uint32_t adb_start_bit_wait_us = ADB_START_BIT_DELAY;
uint8_t mouse_handler_id = MOUSE_DEFAULT_HANDLER_ID;
uint8_t kbd_handler_id = KBD_DEFAULT_HANDLER_ID;
uint8_t mousepending = 0;
uint8_t kbdpending = 0;
uint8_t kbdskip = 0;
uint16_t kbdprev0 = 0;
uint16_t mousereg0 = 0;
uint16_t kbdreg0 = 0;
uint16_t kbdreg2 = 0xFFFF;
uint8_t kbdsrq = 0;
uint8_t mousesrq = 0;
uint16_t modifierkeys = 0xFFFF;
uint64_t kbskiptimer = 0;
bool adb_reset = false;
volatile bool adb_collision = false; 
volatile bool collision_detection = false;
bool mouse_skip_next_listen_reg3 = false;
bool kbd_skip_next_listen_reg3 = false;



extern bool global_debug;
// The original data_lo code would just set the bit as an output
// That works for a host, since the host is doing the pullup on the ADB line,
// but for a device, it won't reliably pull the line low.  We need to actually
// set it.


inline bool AdbInterface::place_bit0(void)
{
  data_lo();
  adb_delay_us(65);
  data_hi();
  return adb_delay_us(35);
}
inline bool AdbInterface::place_bit1(void)
{
  data_lo();
  adb_delay_us(35);
  data_hi();
  return adb_delay_us(65);
}
inline bool AdbInterface::send_byte(uint8_t data)
{
  for (int i = 0; i < 8; i++)
  {
    if (data & (0x80 >> i))
    {
      if(!place_bit1()) return false;
    }
    else
    {
      if(!place_bit0()) return false;
    }
  }
  return true;
}
int16_t AdbInterface::ReceiveCommand(uint8_t srq)
{
  uint8_t bits; 
  uint16_t lo, hi;
  int16_t data = 0;
  static uint32_t attention_reject_count = 0;
  
  // find attention & start bit
  hi = wait_data_lo(adb_start_bit_wait_us); 
  if (!hi)
    return -1;
  do 
  {
    lo = wait_data_hi(4000);
    // IIgs Hardware Reference (Table 6-8): Attention 560–1040 µs; Global Reset >= 2.8 ms.
    // Measured low on RP2040 often reads short of the book minimum; ADB_ATTENTION_LO_MIN_US (default 500).
    if (!lo || lo > 1040 || lo < ADB_ATTENTION_LO_MIN_US)
    {
      if (lo >= 2800)
      {
        adb_reset = true;
        if (global_debug)
        {
          Serial.print("ALL: Global reset detected, wait time was ");
          Serial.print(lo, DEC);
          Serial.println("us");
        }
        return -100;
      }
      else
      {
        // Ignore/noise pulses are expected; avoid flooding debug UART because that can perturb timing.
        if (global_debug)
        {
          attention_reject_count++;
          // Print only occasionally and only when near plausible attention widths.
          if ((lo >= 400 && lo < ADB_ATTENTION_LO_MIN_US) || (lo > 1040 && lo <= 1200) || ((attention_reject_count % 512u) == 0u))
          {
            Serial.print("ADB RX fail: ATTENTION lo=");
            Serial.print(lo, DEC);
            Serial.print(" (count=");
            Serial.print(attention_reject_count, DEC);
            Serial.println(")");
          }
        }
      }
      return -1;

    }
    else
    {
      // Attention was ~800us like it should be
      break;
    }
  }
  while(true);

  // Sync (high) then start bit low: allow 150us for long sync discovery.
  hi = wait_data_lo(150);
  // IIgs Table 6-8 expresses sync as 60-70% of a 70-130us bit-cell => 42-91us envelope.
  // Default keeps slight slack (40-95 us); strict mode uses the pure 42-91 us envelope.
#if ADB_STRICT_SYNC_WINDOW
  if (!hi || hi > 91 || hi < 42)
#else
  if (!hi || hi > 95 || hi < 40)
#endif
  {
    if (global_debug)
    {
      Serial.print("ADB RX fail: SYNC hi=");
      Serial.println(hi, DEC);
    }
    return -3;
  }

  for (bits = 0; bits < 8; bits++)
  {
    lo = wait_data_hi(130);
    if (!lo)
    {
      goto out;
    }
    hi = wait_data_lo(100);
    if (!hi)
    {
      goto out;
    }
    // Bit cell: 70–130 µs (Apple IIgs Hardware Reference)
    uint16_t cell = (uint16_t)(lo + hi);
    if (cell < 70 || 130 < cell)
    {
      goto out;
    }

    data <<= 1;
#if ADB_STRICT_DUTY_CYCLE_DECODE
    // Spec-faithful decode: bit 1 if low <35%, bit 0 if low >65%; reject ambiguous middle duty.
    uint32_t lo_x100 = (uint32_t)lo * 100u;
    uint32_t cell_x35 = 35u * (uint32_t)cell;
    uint32_t cell_x65 = 65u * (uint32_t)cell;
    if (lo_x100 < cell_x35)
    {
      data |= 1;
    }
    else if (lo_x100 > cell_x65)
    {
      /* bit 0: already shifted in */
    }
    else
    {
      goto out;
    }
#else
    // Tolerant midpoint decode (legacy behavior).
    if ((uint32_t)lo * 100u < 50u * (uint32_t)cell)
    {
      data |= 1;
    }
    else
    {
      /* bit 0: already shifted in */
    }
#endif
  }

  if (srq)
  {
    data_lo();
    adb_delay_us(250);
    data_hi();
  }
  else
  {
    // IIgs Hardware Reference: device SRQ is an extension of stop low (>= 140 µs) and table lists 140–260 µs.
    wait_data_hi(400);
  }
  return data;
out:
  if (global_debug)
  {
    Serial.print("ADB RX fail: BIT b=");
    Serial.print(bits, DEC);
    Serial.print(" lo=");
    Serial.print(lo, DEC);
    Serial.print(" hi=");
    Serial.print(hi, DEC);
    Serial.print(" cell=");
    Serial.println((unsigned)(lo + hi), DEC);
  }
  return -4;
}


void AdbInterface::ProcessCommand(int16_t cmd)
{
  uint8_t  listen_addr, listen_handler_id;
  uint16_t mousereg3, kbdreg3;
  int32_t listen_register;

  if (cmd < 0)
  {
    // -1 is waiting for a signal
    // -100 is a 3ms ADB reset
    if (-1 == cmd || -100 == cmd)
    {
      if (cmd == -100) 
      {
        if (global_debug)
        {
          Serial.println("ALL: Global 3ms reset signal");
        }
      }
      return;
    }

    if (global_debug)
    {
      Serial.print("ALL: CMD code error, cmd: ");
      Serial.println(cmd, HEX);
    }
    return;
  }

  if ((0x0F & cmd) == 0x00) 
  {
    adb_reset = true;
    if (global_debug)
    {
      Serial.println("ALL: Cmd for reset all devices");
    }
    return;
  }
  // see if it is addressed to us
  if (((cmd >> 4) & 0x0F) == mouse_addr)
  {
    switch (cmd & 0x0F)
    {
    case 0x1:
      Serial.println("MOUSE: Got FLUSH request");
      break;
    case 0x8:
      Serial.println("MOUSE: Got LISTEN request for register 0");
      break;
    case 0x9:
      Serial.println("MOUSE: Got LISTEN request for register 1");
      break;
    case 0xA:
      Serial.println("MOUSE: Got LISTEN request for register 2");
      break;
    case 0xB:
      listen_register = Receive16bitRegister();
      if (global_debug)
      {
        Serial.print("MOUSE: Got LISTEN request for register 3 at address 0x");
        Serial.println( mouse_addr, HEX);
      }

      if (listen_register >= 0)
      {
        listen_addr = (listen_register >> 8) & 0x0F;
        listen_handler_id = listen_register & 0xFF;
        if (global_debug)
        {
          Serial.print("MOUSE: Listen Register 3 value is 0x");
          Serial.println(listen_register, HEX);
        }
        // self-test
        if (0xFF == listen_handler_id)
        {
          break;
        }
        // Change of address 
        if (0xFE == listen_handler_id )
        {

            if (mouse_skip_next_listen_reg3)
            {
              mouse_skip_next_listen_reg3 = false;
              if (global_debug)
              {
                Serial.print("MOUSE: TALK reg 3 had a collision at 0x");
                Serial.println(mouse_addr, HEX);
              }
              break;
            }
            mouse_addr = listen_addr;
            adb_hub_on_host_address_assigned(true);
            if (global_debug)
            {
              Serial.print("MOUSE: address change to 0x");
              Serial.println(mouse_addr, HEX);
            }
        }
        else
        {
          
          // Don't change address for mouse, handler id values can be 1, 2, or 4.
          //   1 - standard mouse 
          //   2 - standard mouse with extra sensitivity
          //   4 - extended mouse 
          /*  Don't change mouse type from handler id 1 (default)
          if (listen_handler_id == 1 || listen_handler_id == 2)
          {
            mouse_handler_id  = listen_handler_id;
          }
          */
          if (global_debug)
          {
            Serial.print("MOUSE: LSTN Reg3 val is 0x");
            Serial.print(listen_register, HEX);
            Serial.print("@0x");
            Serial.println(mouse_addr, HEX);
            
            Serial.print("MOUSE: handler id change to  0x");
            Serial.println( mouse_handler_id, HEX);
          }
        }
      } 
      else
      {
        if (global_debug)
        {
          
          Serial.print("MOUSE: Listen Register 3 errored with code ");
          Serial.println( listen_register, DEC);
        }
      }
      break;
    case 0xC: // talk register 0

      if (mousepending)
      {
        DetectCollision();
        if (Send16bitRegister(mousereg0))
        {
          ResetCollision();
          mousepending = 0;
          mousesrq = 0;
        }
        else{
          ResetCollision();      
          mousesrq = 1;
          if (global_debug) 
          {
            Serial.print("MOUSE: Collision on sending register 0 on TALK request at address 0x");
            Serial.println(mouse_addr, HEX);
          }  
        }

        break;
      }
      break;
    case 0xD: // talk register 1
      Serial.println("MOUSE: Got TALK request for register 1");
      break;
    case 0xE: // talk register 2
      Serial.println("MOUSE Got TALK request for register 2");
      break;
    case 0xF: // talk register 3
      if (global_debug) 
      {
        Serial.println("MOUSE: Got TALK request for register 3");
      }
      // sets device address
      mousereg3 = GetAdbRegister3Mouse();
      DetectCollision();
      if( Send16bitRegister(mousereg3))
      {
        ResetCollision();
      }
      else
      {
        ResetCollision();
        mouse_skip_next_listen_reg3 = true;
        adb_hub_note_reg3_collision(true);
        if (global_debug)
        {
          Serial.print("MOUSE: Collision TALK register 3 at 0x");
          Serial.println(mouse_addr, HEX);
        }
      }
      if (global_debug)
      {
          Serial.print("MOUSE: Got TALK request for register 3 at address 0x");
          Serial.println( mouse_addr, HEX);
      }
      break;
    default:
      Serial.print("MOUSE: Unknown cmd: 0x");
      Serial.println(cmd, HEX);
      break;
    }
  }
  else
  {
    if (mousepending)
      mousesrq = 1;
  }

  if (((cmd >> 4) & 0x0F) == kbd_addr)
  {
    switch (cmd & 0x0F)
    {
    case 0x1:
      if (global_debug)
      {
        Serial.println("KBD: Got FLUSH request");
      }
      break;
    case 0x8:
      Serial.println("KBD: Got LISTEN request for register 0");
      
      break;
    case 0x9:
      Serial.println("KBD: Got LISTEN request for register 1");
      break;
    case 0xA:
      if (global_debug)
      {
        Serial.println("KBD: Got LISTEN request for register 2");
      }
      listen_register = Receive16bitRegister();
      
      if (KDB_EXTENDED_HANDLER_ID == kbd_handler_id)
      {
        adb_set_leds(listen_register);
      }
      break;
    case 0xB:
      listen_register = Receive16bitRegister();
      if (global_debug)
      {
        Serial.print("KBD: Got LISTEN request for register 3 at address 0x");
        Serial.println(kbd_addr, HEX);
      }
      if (listen_register >= 0)
      {
        listen_addr = (listen_register >> 8) & 0x0F;
        listen_handler_id = listen_register & 0xFF;
        if (global_debug)             
        {
          Serial.print("KBD: Listen Register 3 value is 0x");
          Serial.println(listen_register, HEX);
        }
        // self-test
        if (0xFF == listen_handler_id)
        {
          break;
        }
        // Change of address 
        if (0xFE == listen_handler_id)
        {

            if (kbd_skip_next_listen_reg3)
            {
              kbd_skip_next_listen_reg3 = false;
              if (global_debug)
              {
                Serial.print("KDB: had a collision reg 3 at 0x");
                Serial.println(kbd_addr, HEX);
              }
              break;
            }
            kbd_addr = listen_addr;
            adb_hub_on_host_address_assigned(false);
            if (global_debug)
            {
              Serial.print("KBD: address change to 0x");
              Serial.println(kbd_addr, HEX);
            }

        }
        else
        {
          if (KDB_EXTENDED_HANDLER_ID == listen_handler_id || KBD_DEFAULT_HANDLER_ID == listen_handler_id)
          { 
            kbd_handler_id = listen_handler_id;
          }
          if (global_debug)
          {              
            Serial.print("KBD: address change to 0x");
            Serial.print(kbd_addr, HEX);
            Serial.print(", handler id change to 0x");
            Serial.println(kbd_handler_id, HEX);
          }
        }
      }  
      else
      {
        if (global_debug)
        {
          Serial.print("KBD: Listen Register 3 errored with code ");
          Serial.println(listen_register, DEC);
        }
      }
      
      break;
    case 0xC: // talk register 0
      if (kbdpending)
      {

        if (kbdskip)
        {
          kbdskip = 0;
          // Serial.println("Skipping invalid 255 signal and sending keyup instead");

          // Send a 'key released' code to avoid ADB sticking to the previous key
          kbdprev0 |= 0x80;
          kbdreg0 = (kbdprev0 << 8) | 0xFF;

          // Save timestamp
          kbskiptimer = millis();

        }
        else if (millis() - kbskiptimer < 90)
        {
          // Check timestamp and don't process the key event if it came right after a 255
          // This is meant to avoid a glitch where releasing a key sends 255->keydown instead
          Serial.println("Too little time since bugged keyup, skipping this keydown event");
          kbdpending = 0;
          break;
        }
        DetectCollision();
        if (Send16bitRegister(kbdreg0))
        {
          ResetCollision();
          kbdsrq = 0;
          kbdpending = 0;

        }
        else
        {
          ResetCollision();
          kbdsrq = 1;
          if (global_debug)
          {
            Serial.println("KBD: Collision detected on sending register 0 on TALK request");
          }
        }
      }
      break;
    case 0xD: // talk register 1
      Serial.println("KBD: Got TALK request for register 1");
      break;
    case 0xE: // talk register 2
      if (global_debug) 
      {
        Serial.println("KBD: Got TALK request for register 2");
      }
      DetectCollision();
      if (Send16bitRegister(kbdreg2))
      {
        ResetCollision();
      }
      else
      {
        ResetCollision();
        if (global_debug)
        {
          Serial.println("KBD: Collision detected on sending register 2 on TALK request");
        }
      }
      
      break;
    case 0xF: // talk register 3
      if (global_debug) 
      { 
        Serial.println("KBD: Got TALK request for register 3");
      }
      // sets device address
      kbdreg3 = GetAdbRegister3Keyboard();
      DetectCollision();
      if (Send16bitRegister(kbdreg3))
      {
          ResetCollision();
      }
      else
      {
        ResetCollision();
        kbd_skip_next_listen_reg3 = true;
        if (global_debug)
        {
          Serial.print("KBD: Collision TALK register 3 at 0x");
          Serial.println(kbd_addr, HEX);           
        }
      }
      break;
    default:
      Serial.print("KBD: Unknown cmd: 0x");
      Serial.println(cmd, HEX);           
      break;
    }
  }
  else
  {
    if (kbdpending)
      kbdsrq = 1;
  }
}

uint16_t AdbInterface::GetAdbRegister3Keyboard()
{
  uint16_t kbdreg3 = 0;
  uint8_t addr_field = adb_hub_is_host_assigned(false)
                           ? (uint8_t)(kbd_addr & 0x0F)
                           : adb_hub_propose_reg3_address(kbd_addr, false);
  B_UNSET(kbdreg3, 15);
  B_SET(kbdreg3, 14);
  B_UNSET(kbdreg3, 13);
  B_UNSET(kbdreg3, 12);
  kbdreg3 |= (uint16_t)addr_field << 8;
  kbdreg3 |= kbd_handler_id;

  return kbdreg3;
}
uint16_t AdbInterface::GetAdbRegister3Mouse()
{
  uint16_t mousereg3 = 0;
  uint8_t addr_field = adb_hub_is_host_assigned(true)
                           ? (uint8_t)(mouse_addr & 0x0F)
                           : adb_hub_propose_reg3_address(mouse_addr, true);
  B_UNSET(mousereg3, 15);
  B_SET(mousereg3, 14);
  B_UNSET(mousereg3, 13);
  B_UNSET(mousereg3, 12);
  mousereg3 |= (uint16_t)addr_field << 8;
  mousereg3 |= mouse_handler_id;

  return mousereg3;
}

void AdbInterface::Reset(void)
{
  adb_hub_restore_addresses();
  mouse_handler_id = MOUSE_DEFAULT_HANDLER_ID;
  kbd_handler_id = KBD_DEFAULT_HANDLER_ID;
  kbdreg2 = 0xFFFF;
}
