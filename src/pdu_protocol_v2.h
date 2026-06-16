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
#define PDU_V2_MAX_PAYLOAD_LEN 96U
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
    PDU_V2_STATUS_NOT_IMPLEMENTED = 5U,
    PDU_V2_STATUS_BUSY = 6U
} PDU_V2_Status;

typedef enum
{
    /* Basic link check. No hardware state is touched. */
    PDU_V2_OP_PING = 0x00U,
    /* Ask the PDU what protocol revision and capabilities it supports. */
    PDU_V2_OP_GET_PROTOCOL_INFO = 0x01U,
    /* Compact health packet intended for frequent SOH polling. */
    PDU_V2_OP_GET_SUMMARY_STATUS = 0x02U,
    /* Expose MCU reset reason so the host can report watchdog/brownout history. */
    PDU_V2_OP_GET_RESET_INFO = 0x03U,
    /* Human-readable command/version summary for bench testing. */
    PDU_V2_OP_HELP = 0x04U,
    /* Read back one logical output or all logical outputs. */
    PDU_V2_OP_GET_OUTPUT_STATE = 0x10U,
    /* Set one logical output, then report final state. */
    PDU_V2_OP_SET_OUTPUT_STATE = 0x11U,
    /* Turn a single output off, wait, then restore it on. */
    PDU_V2_OP_POWER_CYCLE_OUTPUT = 0x12U,
    /* Fire one burn-wire channel for a bounded duration, then force it off. */
    PDU_V2_OP_FIRE_BURN_WIRE = 0x13U,
    /* Set a torque-coil H-bridge pair directly; ADCS logic lives on the host. */
    PDU_V2_OP_SET_TORQUE_COIL = 0x14U,
    /* Read one torque-coil H-bridge pair state. */
    PDU_V2_OP_GET_TORQUE_COIL = 0x15U,
    /* Read LTC4012 charger enable/status pins. */
    PDU_V2_OP_GET_CHARGER_STATUS = 0x16U,
    /* Enable or shut down the LTC4012 charger through SHDN. */
    PDU_V2_OP_SET_CHARGER_STATE = 0x17U,
    /* Ask the watchdog task to stop servicing watchdogs so hardware resets the MCU. */
    PDU_V2_OP_SOFTWARE_RESET = 0x20U
} PDU_V2_Opcode;

typedef enum
{
    PDU_OUTPUT_3V3_1 = 0x01U,
    PDU_OUTPUT_3V3_2 = 0x02U,
    PDU_OUTPUT_5V_1 = 0x03U,
    PDU_OUTPUT_5V_2 = 0x04U,
    PDU_OUTPUT_5V_3 = 0x05U,
    /*
     * 5V_4 is the upstream input for the 12 V regulator on this board. It is
     * reserved from public command handling so 12 V control owns SW_5V_EN4.
     */
    PDU_OUTPUT_5V_4_RESERVED = 0x06U,
    PDU_OUTPUT_12V = 0x07U,
    PDU_OUTPUT_VBATT = 0x08U,
    /* Two separate burn-wire deployment channels. */
    PDU_OUTPUT_BURN1 = 0x09U,
    PDU_OUTPUT_BURN2 = 0x0AU,
    /*
     * Coarse H-bridge group output IDs are reserved. Use SET_TORQUE_COIL and
     * GET_TORQUE_COIL for per-coil torque control/readback.
     */
    PDU_OUTPUT_HBRIDGE1 = 0x0BU,
    PDU_OUTPUT_HBRIDGE2 = 0x0CU
} PDU_V2_OutputId;

/* Special ID for all-output readback. It is rejected by output set commands. */
#define PDU_V2_OUTPUT_ALL 0xFFU
#define PDU_V2_OUTPUT_COUNT 9U

/* Capability bits let the host discover which optional features this firmware supports. */
#define PDU_V2_CAP_CRC16 (1U << 0)
#define PDU_V2_CAP_SEQUENCE_ACK (1U << 1)
#define PDU_V2_CAP_RESET_INFO (1U << 2)
#define PDU_V2_CAP_SUMMARY_STATUS (1U << 3)
#define PDU_V2_CAP_POWER_CYCLE (1U << 4)
#define PDU_V2_CAP_SOFTWARE_RESET (1U << 5)
#define PDU_V2_CAP_HELP (1U << 6)
#define PDU_V2_CAP_ACTUATOR_COMMANDS (1U << 7)

#define PDU_V2_FAULT_BITMAP_HBRIDGE1 (1U << 0)
#define PDU_V2_FAULT_BITMAP_HBRIDGE2 (1U << 1)

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

/*
 * PING response payload layout:
 * byte 0: protocol version
 */
#define PDU_V2_PING_LEN 1U

/*
 * POWER_CYCLE_OUTPUT request payload layout:
 * byte 0: output ID
 * byte 1-2: off time in milliseconds, little-endian
 */
#define PDU_V2_POWER_CYCLE_REQ_LEN 3U
#define PDU_V2_POWER_CYCLE_MAX_MS 10000U

/*
 * FIRE_BURN_WIRE request payload layout:
 * byte 0: output ID, either PDU_OUTPUT_BURN1 or PDU_OUTPUT_BURN2
 * byte 1-2: fire time in milliseconds, little-endian
 * byte 3-4: arm token, little-endian. Must equal PDU_V2_BURN_ARM_TOKEN.
 */
#define PDU_V2_FIRE_BURN_REQ_LEN 5U
#define PDU_V2_FIRE_BURN_MAX_MS 10000U
#define PDU_V2_BURN_ARM_TOKEN 0xB142U

typedef enum
{
    PDU_TORQUE_COIL_1 = 1U,
    PDU_TORQUE_COIL_2 = 2U,
    PDU_TORQUE_COIL_3 = 3U,
    PDU_TORQUE_COIL_4 = 4U
} PDU_V2_TorqueCoilId;

typedef enum
{
    PDU_TORQUE_MODE_COAST = 0U,
    PDU_TORQUE_MODE_FORWARD = 1U,
    PDU_TORQUE_MODE_REVERSE = 2U,
    PDU_TORQUE_MODE_BRAKE = 3U
} PDU_V2_TorqueMode;

typedef enum
{
    PDU_TORQUE_CURRENT_100 = 0U,
    PDU_TORQUE_CURRENT_50 = 1U
} PDU_V2_TorqueCurrent;

/*
 * SET_TORQUE_COIL request payload layout:
 * byte 0: coil ID, 1..4
 * byte 1: mode, 0=coast/off, 1=forward, 2=reverse, 3=brake
 * byte 2: current scalar, 0=100%, 1=50%
 * byte 3-4: duration in milliseconds, little-endian. 0 means latch until changed.
 */
#define PDU_V2_SET_TORQUE_COIL_REQ_LEN 5U
#define PDU_V2_GET_TORQUE_COIL_REQ_LEN 1U
#define PDU_V2_TORQUE_COIL_RESP_LEN 5U
#define PDU_V2_TORQUE_COIL_MAX_MS 60000U

/*
 * Charger status response layout:
 * byte 0: charger enabled command/readback, 1=enabled, 0=shutdown
 * byte 1: charge indicator active, 1=CHRG asserted low, 0=not asserted
 * byte 2: SHDN output latch level
 * byte 3: raw CHRG pin level
 *
 * LTC4012 polarity:
 * - SHDN high enables/permits charger operation
 * - SHDN low shuts the charger down
 * - CHRG is an active-low open-drain charge indicator
 */
#define PDU_V2_SET_CHARGER_STATE_REQ_LEN 1U
#define PDU_V2_CHARGER_STATUS_RESP_LEN 4U

#endif /* PDU_PROTOCOL_V2_H */
