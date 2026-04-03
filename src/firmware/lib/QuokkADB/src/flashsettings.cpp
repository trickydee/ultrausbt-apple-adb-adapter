//---------------------------------------------------------------------------
//  BT-USB-ADB-Adapter
//  This project is based on QuokkaADB:
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
#include "flashsettings.h"
#include "pico.h"
#include "pico/multicore.h"
#include "string.h"
#define STORAGE_CMD_TOTAL_BYTES 64

/**
 * Byte offset of the QuokkADB settings sector in external flash.
 * Must match pico-sdk .../pico_btstack/include/pico/btstack_flash_bank.h:
 * BTstack TLV banks start at PICO_FLASH_BANK_STORAGE_OFFSET and span
 * PICO_FLASH_BANK_TOTAL_SIZE (default 2 * FLASH_SECTOR_SIZE).
 * We use exactly one sector immediately below that region — never the same
 * sector as the link-key / TLV store.
 *
 * RP2040: TLV at end-8KiB → settings at third-from-last sector.
 * RP2350+A2: TLV starts one sector higher (last sector reserved) → old
 * "capacity - 3 sectors" overlapped TLV; fixed by using the same formula as BTstack.
 */
static uint32_t settings_flash_offset_bytes(void)
{
#ifndef PICO_FLASH_BANK_TOTAL_SIZE
#define PICO_FLASH_BANK_TOTAL_SIZE (FLASH_SECTOR_SIZE * 2u)
#endif
#if defined(PICO_RP2350) && defined(PICO_RP2350_A2_SUPPORTED) && PICO_RP2350_A2_SUPPORTED
    const uint32_t tlv_base = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE - PICO_FLASH_BANK_TOTAL_SIZE;
#else
    const uint32_t tlv_base = PICO_FLASH_SIZE_BYTES - PICO_FLASH_BANK_TOTAL_SIZE;
#endif
    return tlv_base - FLASH_SECTOR_SIZE;
}

void FlashSettings::init(void)
{
    // JEDEC capacity (for diagnostics; flash layout follows PICO_FLASH_SIZE_BYTES / BTstack)
    uint8_t txbuf[STORAGE_CMD_TOTAL_BYTES] = {0x9f};
    uint8_t rxbuf[STORAGE_CMD_TOTAL_BYTES] = {0};
    uint32_t saved_isr_state = save_and_disable_interrupts();
    flash_do_cmd(txbuf, rxbuf, STORAGE_CMD_TOTAL_BYTES);
    restore_interrupts(saved_isr_state);
    _capacity = 1u << rxbuf[3];
    _last_sector = settings_flash_offset_bytes();

    // Read initial settings
    uint8_t* setting_buffer = read_settings_page();
    
    if (((uint16_t*)setting_buffer)[0] == QUOKKADB_SETTINGS_MAGIC_NUMBER) 
    {
        memcpy((void*)&_settings, setting_buffer, FLASH_PAGE_SIZE);
    }
    else
    {
        // set default values
        _settings.magic_number = QUOKKADB_SETTINGS_MAGIC_NUMBER;
        _settings.led_on = 1;
    }

}

void FlashSettings::write_settings_page(uint8_t *buf)
{
    multicore_lockout_start_blocking();
    uint32_t saved_isr_state = save_and_disable_interrupts();

    flash_range_erase(_last_sector, FLASH_SECTOR_SIZE);
    flash_range_program(_last_sector, buf, FLASH_PAGE_SIZE);

    restore_interrupts(saved_isr_state);
    multicore_lockout_end_blocking();
}

uint8_t* FlashSettings::read_settings_page(void)
{
    return (uint8_t*)(XIP_BASE + _last_sector);
}

void FlashSettings::save(void)
{
    write_settings_page((uint8_t*)&_settings);
}