//---------------------------------------------------------------------------
//  HIDHopper ADB
//  This project is based on adbuino and QuokkADB:
//
//	ADBuino & QuokkaADB ADB keyboard and mouse adapter
//
//	   Copyright (C) 2021-2022 akuker
//     Copyright (C) 2022 Rabbit Hole Computing LLC
//
//  This file is part of the  ADBuino and the QuokkaADB projects.
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
//----------------------------------------------------------------------------


#include "usbmouseparser.h"
#include "usb_hid_keys.h"
#ifdef QUOKKADB
#include "rp2040_serial.h"
using rp2040_serial::Serial;
#endif

extern bool global_debug;

#ifndef ADB_MOUSE_ACCUMULATE_DELTAS
#define ADB_MOUSE_ACCUMULATE_DELTAS 1
#endif

bool MouseRptParser::MouseChanged()
{
    return ((m_movedx != 0) || (m_movedy != 0) || m_mouse_button_changed);
}
int32_t MouseRptParser::GetDeltaX()
{
    return m_movedx;
}
int32_t MouseRptParser::GetDeltaY()
{
    return m_movedy;
}
void MouseRptParser::ResetMouseMovement()
{
    m_movedy = 0;
    m_movedx = 0;
    m_mouse_button_changed = false;
}
bool MouseRptParser::MouseButtonIsPressed()
{
    return m_mouse_left_button_is_pressed || m_mouse_right_button_is_pressed;
}


void MouseRptParser::OnMouseMove(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.print("dx=");
        Serial.print(mi->dX, DEC);
        Serial.print(" dy=");
        Serial.println(mi->dY, DEC);
    }
#if ADB_MOUSE_ACCUMULATE_DELTAS
    auto saturating_add_i8 = [](int8_t current, int8_t delta) -> int8_t {
        int16_t sum = static_cast<int16_t>(current) + static_cast<int16_t>(delta);
        if (sum > 127) return 127;
        if (sum < -128) return -128;
        return static_cast<int8_t>(sum);
    };
    // Accumulate USB/BLE mouse deltas between ADB polls so movement isn't dropped.
    m_movedx = saturating_add_i8(m_movedx, mi->dX);
    m_movedy = saturating_add_i8(m_movedy, mi->dY);
#else
    // Legacy behavior: keep only the latest delta between polls.
    m_movedx = mi->dX;
    m_movedy = mi->dY;
#endif
}
void MouseRptParser::OnLeftButtonUp(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.println("L Butt Up");
    }
    m_mouse_left_button_is_pressed = false;
    m_mouse_button_changed = true;
};
void MouseRptParser::OnLeftButtonDown(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.println("L Butt Dn");
    }
    m_mouse_left_button_is_pressed = true;
    m_mouse_button_changed = true;
};
void MouseRptParser::OnRightButtonUp(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.println("R Butt Up");
    }
    switch (m_right_btn_mode)
    {
        case MouseRightBtnMode::ctrl_click :
            m_mouse_left_button_is_pressed = false;
            m_mouse_button_changed = true;
            // Enqueue Ctrl-up; main loop will send it (no blocking wait here to avoid deadlock)
            m_keyboard->OnKeyUp(0, USB_KEY_LEFTCTRL);
        break;
        case MouseRightBtnMode::right_click :
            m_mouse_right_button_is_pressed = false;
            m_mouse_button_changed = true;
        break;
    }

};
void MouseRptParser::OnRightButtonDown(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.println("R Butt Dn");
    }
    switch (m_right_btn_mode)
    {
        case MouseRightBtnMode::ctrl_click :
            // Enqueue Ctrl-down then left-click; main loop will send them (no blocking wait to avoid deadlock)
            m_keyboard->OnKeyDown(0, USB_KEY_LEFTCTRL);
            m_mouse_left_button_is_pressed = true;
            m_mouse_button_changed = true;
        break;
        case MouseRightBtnMode::right_click :
            m_mouse_right_button_is_pressed = true;
            m_mouse_button_changed = true;
        break;
    }
};
void MouseRptParser::OnMiddleButtonUp(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.println("M Butt Up");
    }

};
void MouseRptParser::OnMiddleButtonDown(MOUSEINFO *mi)
{
    if (global_debug)
    {
        Serial.println("M Butt Dn");
    }
    switch (m_right_btn_mode)
    {
        case MouseRightBtnMode::ctrl_click :
            m_right_btn_mode = MouseRightBtnMode::right_click; 
        break;
        case MouseRightBtnMode::right_click :
            m_right_btn_mode = MouseRightBtnMode::ctrl_click;
        break;
        
    }
};
