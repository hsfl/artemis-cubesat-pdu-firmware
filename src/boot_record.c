/**
 * @file boot_record.c
 * @brief Boot Counter and SAFE-mode Latch Implementation for ATSAME51N19A
 *
 * This module implements a persistent boot record in internal flash memory.
 * It counts consecutive watchdog resets and latches SAFE mode after 3 WDT resets.
 * The record is CRC-protected and can be cleared by command.
 *
 * CRC = CRC32, or Cyclic Redundancy Check 32, is a widely used algorithm for detecting errors in data transmission or storage. 
 * It calculates a 32-bit checksum (a unique value) of the data, which can be used to verify that the data has not been altered 
 * during transmission or storage. If the checksums match at the sender and receiver ends, it indicates that the data has 
 * likely been transmitted or stored correctly.
 * 
 * Latches Safe Mode = If the WDT keeps tripping, just keep the cubesat/PDU in safe mode until manually intervention by Mission Ops.
 *
 * Key Features:
 * - Stores a record in the last flash page (no extra hardware required)
 * - CRC-32 protection for data integrity
 * - Survives all resets and power cycles
 * - Field-clearable via command
 * - Designed for easy review and modification by students/engineers
 *
 * How to Adapt:
 * - To change the number of WDT resets before SAFE latch, modify the threshold in Bootrec_Init()
 * - To add new fields, update bootrec_t and CRC calculation
 * - To change flash location, update linker script and section attribute
 *
 * See boot_record.h for the record structure and API.
 */

#include "boot_record.h"
#include <string.h>
#include "definitions.h" // SYS function prototypes

/**
 * @brief Persistent boot record stored in flash
 *
 * This variable is placed in a special section at the end of flash.
 * It is initialized with default values and only updated by flash operations.
 */
__attribute__((section(".boot_record"), used))
const bootrec_t bootrec_flash = {BOOTREC_SIGNATURE, 0u, 0u, 0u, {0}};

/**
 * @brief RAM mirror of the boot record
 *
 * All operations are performed on this copy before writing back to flash.
 */
bootrec_t mirror;

/**
 * @brief Calculate CRC-32 (polynomial 0xEDB88320)
 *
 * @param data Pointer to data
 * @param len  Length in bytes
 * @return CRC-32 value
 *
 * This function computes a standard CRC-32 over the input data.
 * Used to protect the boot record against corruption.
 */
static uint32_t crc32(const void *data, size_t len)
{
    uint32_t crc = ~0u;
    const uint8_t *p = data;
    while (len--)
    {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
    }
    return ~crc;
}

/**
 * @brief Safely update the boot record in flash
 *
 * This function:
 * 1. Updates the CRC in the RAM mirror
 * 2. Disables interrupts for atomicity
 * 3. Erases the flash block containing the record
 * 4. Writes the updated record to flash
 * 5. Re-enables interrupts
 *
 * WARNING: Flash erase/write is slow and should not be called frequently.
 * Only call when the record has changed.
 *
 * If you change the record structure, update the CRC calculation accordingly.
 */
static void Bootrec_FlashUpdate(void)
{
    mirror.crc32 = crc32(&mirror.boot_cnt, sizeof(mirror) - 8);

    __disable_irq();
    NVMCTRL_REGS->NVMCTRL_ADDR = (uint32_t)&bootrec_flash;
    NVMCTRL_REGS->NVMCTRL_CTRLA = (0x1 | (0xA5u << 8)); // Erase block
    while (!(NVMCTRL_REGS->NVMCTRL_INTFLAG & NVMCTRL_INTFLAG_DONE_Msk))
        ;

    for (uint32_t i = 0; i < sizeof(mirror) / 4; ++i)
        ((volatile uint32_t *)&bootrec_flash)[i] = ((uint32_t *)&mirror)[i];

    NVMCTRL_REGS->NVMCTRL_CTRLA = (0x3 | (0xA5u << 8)); // Write page
    while (!(NVMCTRL_REGS->NVMCTRL_INTFLAG & NVMCTRL_INTFLAG_DONE_Msk))
        ;
    __enable_irq();
}

/**
 * @brief Initialize the boot record and update boot count
 *
 * This function should be called early at startup, before the scheduler starts.
 *
 * Steps:
 * 1. Validates the flash record using the signature and CRC
 * 2. Loads the record into the RAM mirror (or resets if invalid)
 * 3. Checks the reset cause (WDT or not)
 * 4. Increments boot count if WDT reset, or resets count on cold reset
 * 5. Latches SAFE mode if boot count >= 3
 * 6. Writes back to flash if anything changed
 *
 * To change the SAFE latch threshold, modify the '3' in the code below.
 */
void Bootrec_Init(void)
{
    const bootrec_t *f = &bootrec_flash;
    if (f->signature != BOOTREC_SIGNATURE ||
        crc32(&f->boot_cnt, sizeof(bootrec_t) - 8) != f->crc32)
    {
        memset(&mirror, 0, sizeof mirror);
    }
    else
        mirror = *f;

    // Check if last reset was caused by the Watchdog Timer (WDT)
    uint8_t rcause = (RSTC_REGS->RSTC_RCAUSE & RSTC_RCAUSE_WDT_Msk) ? 1 : 0;
    if (rcause)
        mirror.boot_cnt++;
    else
        mirror.boot_cnt = 0;

    // Latch SAFE mode if 3 or more consecutive WDT resets
    if (mirror.boot_cnt >= 3)
        mirror.safe_latch = 1;

    Bootrec_FlashUpdate();
}

/**
 * @brief Clear the SAFE latch and boot count (e.g., from ground command)
 *
 * This function resets the boot count and SAFE latch in the RAM mirror,
 * then writes the cleared record back to flash.
 *
 * Call this from a command handler to allow remote recovery from SAFE mode.
 */
void CMD_ClearSafeLatch(void)
{
    mirror.boot_cnt = 0;
    mirror.safe_latch = 0;
    Bootrec_FlashUpdate();
}

/**
 * @brief Get a pointer to the current RAM mirror of the boot record
 *
 * @return Pointer to bootrec_t mirror
 *
 * Use this to read the current boot count or SAFE latch status from other code.
 */
const bootrec_t *Bootrec_GetMirror(void) { return &mirror; }