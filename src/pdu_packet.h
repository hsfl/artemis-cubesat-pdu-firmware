#ifndef _PDU_PACKET_H    /* Guard against multiple inclusion */
#define _PDU_PACKET_H

#include "string.h"
#include "stdint.h"

/* ASCII offset for command values (48 = '0' in ASCII)
 * Used to convert between ASCII digits and numeric values:
 * - ASCII to value: subtract PDU_CMD_OFFSET
 * - Value to ASCII: add PDU_CMD_OFFSET
 */
#define PDU_CMD_OFFSET 48
#define PDU_MAX_PACKET_SIZE 16  // Size of largest packet (pdu_telem: 1B type + 15B sw_state)

enum PDU_Type
{
    PDU_TYPE_NOP,
    PDU_TYPE_COMMAND_PING,
    PDU_TYPE_COMMAND_SET_SWITCH,
    PDU_TYPE_COMMAND_GET_SWITCH_STATUS,
    PDU_TYPE_DATA_PONG,
    PDU_TYPE_DATA_SWITCH_STATUS,
    PDU_TYPE_DATA_SWITCH_TELEM,
};
typedef uint8_t PDU_Type;

enum PDU_SW
{
    PDU_SW_NONE,
    PDU_SW_ALL,
    PDU_SW_3V3_1,
    PDU_SW_3V3_2,
    PDU_SW_5V_1,
    PDU_SW_5V_2,
    PDU_SW_5V_3,
    PDU_SW_5V_4,
    PDU_SW_12V,
    PDU_SW_VBATT,
    PDU_SW_HBRIDGE1,
    PDU_SW_HBRIDGE2,
    PDU_SW_BURN,
    PDU_SW_BURN1,
    PDU_SW_BURN2
};
typedef uint8_t PDU_SW;

struct __attribute__((packed)) pdu_packet
{
    PDU_Type type;
    PDU_SW sw;
    uint8_t sw_state;
};

struct __attribute__ ((packed)) pdu_telem
{
    PDU_Type type;
    uint8_t sw_state[15];  // Increased to 15 to include None, All, and all switches
};

void decode_pdu_packet(const char *input);
void enableAllGPIOs(void);
void disableAllGPIOs(void);

#endif /* _PDU_PACKET_H */

/* *****************************************************************************
 End of File
 */