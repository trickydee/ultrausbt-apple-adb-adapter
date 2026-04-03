//---------------------------------------------------------------------------
//  BT-USB-ADB-Adapter
//  This project is based on QuokkADB:
//
//	QuokkaADB ADB keyboard and mouse adapter
//
//	   Copyright (C) 2017 bbraun
//	   Copyright (C) 2021-2022 akuker
//     Copyright (C) 2022 Rabbit Hole Computing LLC
//
//  This file is part of the QuokkaADB project.
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
//---------------------------------------------------------------------------
#pragma once

// Version: BT-USB-ADB-Adapter uses version from CMake (BT_USB_ADB_ADAPTER_VERSION_STRING). Otherwise QuokkADB defaults.
#ifdef BT_USB_ADB_ADAPTER_VERSION_STRING
#define PLATFORM_FW_VERSION BT_USB_ADB_ADAPTER_VERSION_STRING
#define PRODUCT_NAME "BT-USB-ADB-Adapter"
#else
#define FW_VER_NUM      "0.2.4"
#define FW_VER_SUFFIX   "beta"
#define PLATFORM_FW_VERSION FW_VER_NUM "-" FW_VER_SUFFIX
#define PRODUCT_NAME "Blue-QuokkADB"
#endif
#define PLATFORM_FW_VER_STRING PRODUCT_NAME " firmware: " PLATFORM_FW_VERSION " " __DATE__ " " __TIME__ " "

