/**
 * @file pdu_packet.h
 * @brief Power Distribution Unit (PDU) Packet Protocol Definitions
 * 
 * This module defines the packet protocol used for communication with the PDU.
 * It handles command packets for controlling power switches and retrieving
 * switch status information. The protocol uses ASCII-based encoding for
 * simplicity and human readability.
 * 
 * Packet Structure:
 * - All packets are ASCII-encoded with a base offset of 48 ('0')
 * - Basic packet format: [Type][Switch][State]
 * - Telemetry packet format: [Type][Switch States Array]
 * 
 * @author Artemis CubeSat Team
 * @date 2024
 */

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

/**
 * @brief PDU Packet Types
 * 
 * Defines the different types of packets that can be sent/received:
 * - Command packets: Used to control the PDU
 * - Data packets: Used to report status and telemetry
 */
enum PDU_Type
{
    PDU_TYPE_NOP,                    ///< No operation
    PDU_TYPE_COMMAND_PING,           ///< Ping command to check PDU connectivity
    PDU_TYPE_COMMAND_SET_SWITCH,     ///< Command to set a switch state
    PDU_TYPE_COMMAND_GET_SWITCH_STATUS, ///< Command to get switch status
    PDU_TYPE_DATA_PONG,              ///< Response to ping
    PDU_TYPE_DATA_SWITCH_STATUS,     ///< Response with single switch status
    PDU_TYPE_DATA_SWITCH_TELEM,      ///< Response with all switch states
};
typedef uint8_t PDU_Type;

/**
 * @brief PDU Switch Identifiers
 * 
 * Defines the different power switches and control interfaces available:
 * - Power rails (3.3V, 5V, 12V)
 * - Battery control
 * - H-bridge motor controllers
 * - Burn wire systems
 */
enum PDU_SW
{
    PDU_SW_NONE,     ///< No switch selected
    PDU_SW_ALL,      ///< All switches
    PDU_SW_3V3_1,    ///< 3.3V rail 1
    PDU_SW_3V3_2,    ///< 3.3V rail 2
    PDU_SW_5V_1,     ///< 5V rail 1
    PDU_SW_5V_2,     ///< 5V rail 2
    PDU_SW_5V_3,     ///< 5V rail 3
    PDU_SW_5V_4,     ///< 5V rail 4
    PDU_SW_12V,      ///< 12V rail
    PDU_SW_VBATT,    ///< Battery control
    PDU_SW_HBRIDGE1, ///< H-bridge controller 1
    PDU_SW_HBRIDGE2, ///< H-bridge controller 2
    PDU_SW_BURN,     ///< Both burn wire systems
    PDU_SW_BURN1,    ///< Burn wire system 1
    PDU_SW_BURN2     ///< Burn wire system 2
};
typedef uint8_t PDU_SW;

/**
 * @brief Basic PDU Packet Structure
 * 
 * Standard packet format for commands and single-switch responses.
 * Packed to ensure consistent size and alignment.
 */
struct __attribute__((packed)) pdu_packet
{
    PDU_Type type;    ///< Packet type (command or data)
    PDU_SW sw;        ///< Target switch identifier
    uint8_t sw_state; ///< Switch state (0=off, 1=on)
};

/**
 * @brief PDU Telemetry Packet Structure
 * 
 * Extended packet format for reporting all switch states.
 * Used in response to PDU_TYPE_COMMAND_GET_SWITCH_STATUS with PDU_SW_ALL.
 */
struct __attribute__ ((packed)) pdu_telem
{
    PDU_Type type;           ///< Packet type (always PDU_TYPE_DATA_SWITCH_TELEM)
    uint8_t sw_state[15];    ///< Array of all switch states
};

/**
 * @brief Decodes and processes incoming PDU packets
 * 
 * @param input Pointer to null-terminated input string containing the packet
 * 
 * This function:
 * 1. Validates the input packet
 * 2. Decodes the packet type and parameters
 * 3. Executes the requested command
 * 4. Sends appropriate response if required
 */
void decode_pdu_packet(const char *input);

/**
 * @brief Enables all PDU GPIO outputs
 * 
 * Sets all power switches and control interfaces to their active state.
 * Use with caution as this will power all systems simultaneously.
 */
void enableAllGPIOs(void);

/**
 * @brief Disables all PDU GPIO outputs
 * 
 * Sets all power switches and control interfaces to their inactive state.
 * This is a safe state that powers down all systems.
 */
void disableAllGPIOs(void);

#endif /* _PDU_PACKET_H */

/* *****************************************************************************
 End of File
 */