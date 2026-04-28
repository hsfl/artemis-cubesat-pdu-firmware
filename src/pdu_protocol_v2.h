#ifndef PDU_PROTOCOL_V2_H
#define PDU_PROTOCOL_V2_H

#include <stdint.h>

/*
 * Fresh framed protocol for the Artemis PDU MCU.
 *
 * Terminology:
 * - SOF  = "start of frame". A single marker byte that tells the parser where
 *          a new message begins on the UART stream.
 * - CRC  = "cyclic redundancy check". A short checksum used to detect
 *          corrupted bytes caused by UART noise or framing loss.
 * - opcode = the operation being requested, for example "get summary status".
 * - seq = "sequence number". The host chooses it, and the PDU copies it back
 *         in the response so the host can match replies to requests.
 *
 * Frame layout:
 * byte 0: SOF (0xA5)
 * byte 1: protocol version
 * byte 2: message type
 * byte 3: opcode
 * byte 4: sequence number
 * byte 5: status/flags
 * byte 6: payload length in bytes
 * byte 7..N: payload
 * final 2 bytes: CRC-16/CCITT over bytes 1..N
 */
#define PDU_V2_VERSION 2U
#define PDU_V2_SOF 0xA5U
#define PDU_V2_MAX_PAYLOAD_LEN 32U
#define PDU_V2_HEADER_LEN 7U
#define PDU_V2_CRC_LEN 2U
#define PDU_V2_MAX_FRAME_LEN (PDU_V2_HEADER_LEN + PDU_V2_MAX_PAYLOAD_LEN + PDU_V2_CRC_LEN)

typedef enum
{
    PDU_V2_MSG_REQUEST = 0U,
    PDU_V2_MSG_RESPONSE = 1U,
    /* Reserved for future asynchronous reports such as boot or fault events. */
    PDU_V2_MSG_EVENT = 2U
} PDU_V2_MessageType;

typedef enum
{
    PDU_V2_STATUS_OK = 0U,
    PDU_V2_STATUS_BAD_OPCODE = 1U,
    PDU_V2_STATUS_BAD_LENGTH = 2U,
    PDU_V2_STATUS_BAD_PARAM = 3U,
    PDU_V2_STATUS_HW_FAULT = 4U,
    PDU_V2_STATUS_NOT_IMPLEMENTED = 5U
} PDU_V2_Status;

typedef enum
{
    /* Ask the PDU what protocol revision and capabilities it supports. */
    PDU_V2_OP_GET_PROTOCOL_INFO = 0x01U,
    /* Compact health packet intended for frequent SOH polling. */
    PDU_V2_OP_GET_SUMMARY_STATUS = 0x02U,
    /* Expose MCU reset reason so the host can report watchdog/brownout history. */
    PDU_V2_OP_GET_RESET_INFO = 0x03U,
    /* Read back one logical output or all logical outputs. */
    PDU_V2_OP_GET_OUTPUT_STATE = 0x10U,
    /* Set one logical output or all logical outputs, then report final state. */
    PDU_V2_OP_SET_OUTPUT_STATE = 0x11U
} PDU_V2_Opcode;

typedef enum
{
    PDU_OUTPUT_3V3_1 = 0x01U,
    PDU_OUTPUT_3V3_2 = 0x02U,
    PDU_OUTPUT_5V_1 = 0x03U,
    PDU_OUTPUT_5V_2 = 0x04U,
    PDU_OUTPUT_5V_3 = 0x05U,
    PDU_OUTPUT_5V_4 = 0x06U,
    PDU_OUTPUT_12V = 0x07U,
    PDU_OUTPUT_VBATT = 0x08U,
    PDU_OUTPUT_BURN1 = 0x09U,
    PDU_OUTPUT_BURN2 = 0x0AU,
    PDU_OUTPUT_HBRIDGE1 = 0x0BU,
    PDU_OUTPUT_HBRIDGE2 = 0x0CU
} PDU_V2_OutputId;

/* Special broadcast ID for bench bring-up and "set/read everything" requests. */
#define PDU_V2_OUTPUT_ALL 0xFFU
#define PDU_V2_OUTPUT_COUNT 12U

/* Capability bits let the host discover which optional features this firmware supports. */
#define PDU_V2_CAP_CRC16 (1U << 0)
#define PDU_V2_CAP_SEQUENCE_ACK (1U << 1)
#define PDU_V2_CAP_RESET_INFO (1U << 2)
#define PDU_V2_CAP_SUMMARY_STATUS (1U << 3)

#define PDU_V2_FAULT_BITMAP_NONE 0x00U

/*
 * Summary status payload layout:
 * byte 0-1: output enable bitmap in output-ID order
 * byte 2:   reset cause (RSTC_RCAUSE)
 * byte 3:   fault bitmap (reserved, currently 0)
 * byte 4-7: uptime in seconds, little-endian
 * byte 8:   capability bitmap
 */
#define PDU_V2_SUMMARY_STATUS_LEN 9U

/*
 * Protocol info payload layout:
 * byte 0: protocol version
 * byte 1: capability bitmap
 * byte 2: max payload length
 * byte 3: output count
 * byte 4: firmware major
 * byte 5: firmware minor
 * byte 6: firmware patch
 */
#define PDU_V2_PROTOCOL_INFO_LEN 7U

#endif /* PDU_PROTOCOL_V2_H */
