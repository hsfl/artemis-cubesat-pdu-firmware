/**
 * @file boot_record.h
 * @brief Persistent Boot Record Structure and API
 *
 * This header defines the structure and API for a persistent boot record
 * stored in internal flash. The record is used to count consecutive WDT resets
 * and latch SAFE mode after a threshold. It is CRC-protected and can be cleared.
 *
 * How to Adapt:
 * - To add new fields, extend bootrec_t and update CRC logic in boot_record.c
 * - To change the SAFE latch threshold, modify Bootrec_Init() in boot_record.c
 * - To change the flash location, update the linker script and section attribute
 *
 * Usage:
 * 1. Call Bootrec_Init() at startup (before scheduler)
 * 2. Check Bootrec_GetMirror()->safe_latch to force SAFE mode
 * 3. Call CMD_ClearSafeLatch() to clear the latch and counter
 */

#ifndef BOOT_RECORD_H
#define BOOT_RECORD_H

#include <stdint.h>

/**
 * @brief Signature value to identify a valid boot record in flash
 *
 * If this value is not present, the record is considered uninitialized or erased.
 */
#define BOOTREC_SIGNATURE 0xA55AA55Au

/**
 * @brief Persistent boot record structure (stored in flash)
 *
 * Fields:
 * - signature:   Identifies a valid record (should be BOOTREC_SIGNATURE)
 * - crc32:       CRC-32 of the next fields (protects against corruption)
 * - boot_cnt:    Number of consecutive WDT resets
 * - safe_latch:  1 = force SAFE mode at startup
 * - rsvd[12]:    Reserved/padding (can be used for future fields)
 *
 * To add new fields, insert them before rsvd and update CRC logic.
 */
typedef struct {
    uint32_t signature;   /* BOOTREC_SIGNATURE: identifies valid record */
    uint32_t crc32;       /* CRC of the next fields                    */
    uint32_t boot_cnt;    /* # of consecutive WDT boots                */
    uint32_t safe_latch;  /* 1 = force SAFE mode                       */
    uint32_t rsvd[12];    /* Pad to full 64-byte cache line            */
} bootrec_t;

/**
 * @brief Initialize the boot record and update boot count
 *
 * Call this at startup before the scheduler. Updates the boot count and
 * latches SAFE mode if needed. Writes back to flash if anything changed.
 */
void Bootrec_Init(void);

/**
 * @brief Clear the SAFE latch and boot count (e.g., from ground command)
 *
 * Resets the boot count and SAFE latch, then writes the cleared record to flash.
 */
void CMD_ClearSafeLatch(void);

/**
 * @brief Get a pointer to the current RAM mirror of the boot record
 *
 * Use this to read the current boot count or SAFE latch status from other code.
 */
const bootrec_t *Bootrec_GetMirror(void);

#endif /* BOOT_RECORD_H */ 