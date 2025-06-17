/**
 * @file pdu_packet.c
 * @brief Power Distribution Unit (PDU) Packet Protocol Implementation
 * 
 * This module implements the packet protocol for the PDU, handling:
 * - Command packet decoding and processing
 * - Switch state control
 * - Status reporting and telemetry
 * 
 * The implementation uses ASCII-based encoding for simplicity and
 * human readability, with a base offset of 48 ('0') for all values.
 * 
 * @author Artemis CubeSat Team
 * @date 2024
 */

#include "pdu_packet.h"
#include "definitions.h"

/**
 * @brief Decodes and processes incoming PDU packets
 * 
 * This function implements the main packet processing logic:
 * 1. Validates input packet length and content
 * 2. Decodes packet type and parameters
 * 3. Executes the requested command
 * 4. Sends appropriate response if required
 * 
 * Packet Types Handled:
 * - PING: Responds with PONG to verify connectivity
 * - SET_SWITCH: Controls individual or all power switches
 * - GET_SWITCH_STATUS: Reports current switch states
 * 
 * @param input Pointer to null-terminated input string containing the packet
 */
void decode_pdu_packet(const char *input)
{
    // Return early if input is too short (need at least 3 bytes)
    if (!input || !input[0] || !input[1] || !input[2]) {
        return;
    }

    static uint8_t tx_buf[PDU_MAX_PACKET_SIZE]; // Large enough for both pdu_packet (3B) and pdu_telem (13B)
    struct pdu_packet packet;
    packet.type = input[0] - PDU_CMD_OFFSET;
    packet.sw = input[1] - PDU_CMD_OFFSET;
    packet.sw_state = input[2] - PDU_CMD_OFFSET;

    switch (packet.type)
    {
    case PDU_TYPE_COMMAND_PING:
        // Respond to ping with pong, maintaining the same switch and state values
        packet.type = PDU_TYPE_DATA_PONG + PDU_CMD_OFFSET;
        packet.sw += PDU_CMD_OFFSET;
        packet.sw_state += PDU_CMD_OFFSET;
        memcpy(tx_buf, &packet, sizeof(struct pdu_packet));
        SERCOM3_USART_Write(tx_buf, sizeof(struct pdu_packet));
        SERCOM3_USART_Write("\r\n", 2);
        break;

    case PDU_TYPE_COMMAND_SET_SWITCH:
        if (packet.sw_state == 1)
        {
            // Enable the requested switch(es)
            switch (packet.sw)
            {
            case PDU_SW_ALL:
                enableAllGPIOs();
                break;
            case PDU_SW_3V3_1:
                SW_3V3_EN1_Set();
                break;
            case PDU_SW_3V3_2:
                SW_3V3_EN2_Set();
                break;
            case PDU_SW_5V_1:
                SW_5V_EN1_Set();
                break;
            case PDU_SW_5V_2:
                SW_5V_EN2_Set();
                break;
            case PDU_SW_5V_3:
                SW_5V_EN3_Set();
                break;
            case PDU_SW_5V_4:
                SW_5V_EN4_Set();
                break;
            case PDU_SW_12V:
                // 12V rail requires both 12V and 5V_EN4 to be enabled
                SW_12V_EN1_Set();
                SW_5V_EN4_Set();
                break;
            case PDU_SW_VBATT:
                SW_VBATT_EN_Set();
                break;
            case PDU_SW_HBRIDGE1:
                // Enable all H-bridge 1 control signals
                FAULT1_Set();
                IN1_Set();
                IN2_Set();
                IN3_Set();
                IN4_Set();
                TRQ1_Set();
                SLEEP1_Set();
                break;
            case PDU_SW_HBRIDGE2:
                // Enable all H-bridge 2 control signals
                FAULT2_Set();
                IN5_Set();
                IN6_Set();
                IN7_Set();
                IN8_Set();
                TRQ2_Set();
                SLEEP2_Set();
                break;
            case PDU_SW_BURN:
                // Enable both burn wire systems
                BURN1_EN_Set();
                BURN2_EN_Set();
                BURN_5V_Set();
                break;
            case PDU_SW_BURN1:
                // Enable burn wire system 1
                BURN1_EN_Set();
                BURN_5V_Set();
                break;
            case PDU_SW_BURN2:
                // Enable burn wire system 2
                BURN2_EN_Set();
                BURN_5V_Set();
                break;
            default:
                break;
            }
        }
        else
        {
            // Disable the requested switch(es)
            switch (packet.sw)
            {
            case PDU_SW_ALL:
                disableAllGPIOs();
                break;
            case PDU_SW_3V3_1:
                SW_3V3_EN1_Clear();
                break;
            case PDU_SW_3V3_2:
                SW_3V3_EN2_Clear();
                break;
            case PDU_SW_5V_1:
                SW_5V_EN1_Clear();
                break;
            case PDU_SW_5V_2:
                SW_5V_EN2_Clear();
                break;
            case PDU_SW_5V_3:
                SW_5V_EN3_Clear();
                break;
            case PDU_SW_5V_4:
                SW_5V_EN4_Clear();
                break;
            case PDU_SW_12V:
                // Disable both 12V and 5V_EN4
                SW_12V_EN1_Clear();
                SW_5V_EN4_Clear();
                break;
            case PDU_SW_VBATT:
                SW_VBATT_EN_Clear();
                break;
            case PDU_SW_HBRIDGE1:
                // Disable all H-bridge 1 control signals
                FAULT1_Clear();
                IN1_Clear();
                IN2_Clear();
                IN3_Clear();
                IN4_Clear();
                TRQ1_Clear();
                SLEEP1_Clear();
                break;
            case PDU_SW_HBRIDGE2:
                // Disable all H-bridge 2 control signals
                FAULT2_Clear();
                IN5_Clear();
                IN6_Clear();
                IN7_Clear();
                IN8_Clear();
                TRQ2_Clear();
                SLEEP2_Clear();
                break;
            case PDU_SW_BURN:
                // Disable both burn wire systems
                BURN1_EN_Clear();
                BURN2_EN_Clear();
                BURN_5V_Clear();
                break;
            case PDU_SW_BURN1:
                // Disable burn wire system 1, but only disable 5V if BURN2 is also off
                if (!PORT_PinRead(BURN2_EN_PIN))
                    BURN_5V_Clear();
                BURN1_EN_Clear();
                break;
            case PDU_SW_BURN2:
                // Disable burn wire system 2, but only disable 5V if BURN1 is also off
                if (!PORT_PinRead(BURN1_EN_PIN))
                    BURN_5V_Clear();
                BURN2_EN_Clear();
                break;
            default:
                break;
            }
        }
        break;

    case PDU_TYPE_COMMAND_GET_SWITCH_STATUS:
        if (packet.sw == PDU_SW_ALL)
        {
            // Generate telemetry packet with all switch states
            struct pdu_telem telem;
            telem.type = PDU_TYPE_DATA_SWITCH_TELEM + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_NONE] = 0 + PDU_CMD_OFFSET; // None state
            telem.sw_state[PDU_SW_ALL] = 1 + PDU_CMD_OFFSET;  // All state
            telem.sw_state[PDU_SW_3V3_1] = PORT_PinRead(SW_3V3_EN1_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_3V3_2] = PORT_PinRead(SW_3V3_EN2_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_5V_1] = PORT_PinRead(SW_5V_EN1_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_5V_2] = PORT_PinRead(SW_5V_EN2_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_5V_3] = PORT_PinRead(SW_5V_EN3_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_5V_4] = PORT_PinRead(SW_5V_EN4_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_12V] = (PORT_PinRead(SW_12V_EN1_PIN) && PORT_PinRead(SW_5V_EN4_PIN)) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_VBATT] = PORT_PinRead(SW_VBATT_EN_PIN) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_HBRIDGE1] = (PORT_PinRead(FAULT1_PIN) &&
                                        PORT_PinRead(IN1_PIN) &&
                                        PORT_PinRead(IN2_PIN) &&
                                        PORT_PinRead(IN3_PIN) &&
                                        PORT_PinRead(IN4_PIN) &&
                                        PORT_PinRead(TRQ1_PIN) &&
                                        PORT_PinRead(SLEEP1_PIN)) +
                                       PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_HBRIDGE2] = (PORT_PinRead(FAULT2_PIN) &&
                                        PORT_PinRead(IN5_PIN) &&
                                        PORT_PinRead(IN6_PIN) &&
                                        PORT_PinRead(IN7_PIN) &&
                                        PORT_PinRead(IN8_PIN) &&
                                        PORT_PinRead(TRQ2_PIN) &&
                                        PORT_PinRead(SLEEP2_PIN)) +
                                       PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_BURN] = (PORT_PinRead(BURN1_EN_PIN) &&
                                    PORT_PinRead(BURN2_EN_PIN) &&
                                    PORT_PinRead(BURN_5V_PIN)) +
                                   PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_BURN1] = (PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN_5V_PIN)) + PDU_CMD_OFFSET;
            telem.sw_state[PDU_SW_BURN2] = (PORT_PinRead(BURN2_EN_PIN) && PORT_PinRead(BURN_5V_PIN)) + PDU_CMD_OFFSET;
            memcpy(tx_buf, &telem, sizeof(struct pdu_telem));
            SERCOM3_USART_Write(tx_buf, sizeof(struct pdu_telem));
            SERCOM3_USART_Write("\r\n", 2);
            break;
        }
        // Generate single switch status response
        packet.type = PDU_TYPE_DATA_SWITCH_STATUS + PDU_CMD_OFFSET;
        switch (packet.sw)
        {
        case PDU_SW_3V3_1:
            packet.sw_state = PORT_PinRead(SW_3V3_EN1_PIN);
            break;
        case PDU_SW_3V3_2:
            packet.sw_state = PORT_PinRead(SW_3V3_EN2_PIN);
            break;
        case PDU_SW_5V_1:
            packet.sw_state = PORT_PinRead(SW_5V_EN1_PIN);
            break;
        case PDU_SW_5V_2:
            packet.sw_state = PORT_PinRead(SW_5V_EN2_PIN);
            break;
        case PDU_SW_5V_3:
            packet.sw_state = PORT_PinRead(SW_5V_EN3_PIN);
            break;
        case PDU_SW_5V_4:
            packet.sw_state = PORT_PinRead(SW_5V_EN4_PIN);
            break;
        case PDU_SW_12V:
            // 12V status requires both 12V and 5V_EN4 to be enabled
            packet.sw_state = PORT_PinRead(SW_12V_EN1_PIN) && PORT_PinRead(SW_5V_EN4_PIN);
            break;
        case PDU_SW_VBATT:
            packet.sw_state = PORT_PinRead(SW_VBATT_EN_PIN);
            break;
        case PDU_SW_HBRIDGE1:
            // H-bridge 1 status requires all control signals to be enabled
            packet.sw_state = PORT_PinRead(FAULT1_PIN) &&
                              PORT_PinRead(IN1_PIN) &&
                              PORT_PinRead(IN2_PIN) &&
                              PORT_PinRead(IN3_PIN) &&
                              PORT_PinRead(IN4_PIN) &&
                              PORT_PinRead(TRQ1_PIN) &&
                              PORT_PinRead(SLEEP1_PIN);
            break;
        case PDU_SW_HBRIDGE2:
            // H-bridge 2 status requires all control signals to be enabled
            packet.sw_state = PORT_PinRead(FAULT2_PIN) &&
                              PORT_PinRead(IN5_PIN) &&
                              PORT_PinRead(IN6_PIN) &&
                              PORT_PinRead(IN7_PIN) &&
                              PORT_PinRead(IN8_PIN) &&
                              PORT_PinRead(TRQ2_PIN) &&
                              PORT_PinRead(SLEEP2_PIN);
            break;
        case PDU_SW_BURN:
            // Burn status requires both burn enables and 5V to be enabled
            packet.sw_state = PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN2_EN_PIN) && PORT_PinRead(BURN_5V_PIN);
            break;
        case PDU_SW_BURN1:
            // Burn1 status requires BURN1_EN and 5V to be enabled
            packet.sw_state = PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN_5V_PIN);
            break;
        case PDU_SW_BURN2:
            // Burn2 status requires BURN2_EN and 5V to be enabled
            packet.sw_state = PORT_PinRead(BURN2_EN_PIN) && PORT_PinRead(BURN_5V_PIN);
            break;
        default:
            break;
        }
        packet.sw += PDU_CMD_OFFSET;
        packet.sw_state += PDU_CMD_OFFSET;
        memcpy(tx_buf, &packet, sizeof(struct pdu_packet));
        SERCOM3_USART_Write(tx_buf, sizeof(struct pdu_packet));
        SERCOM3_USART_Write("\r\n", 2);
        break;
    default:
        break;
    };
}

/**
 * @brief Enables all PDU GPIO outputs
 * 
 * This function sets all power switches and control interfaces to their active state.
 * It should be used with caution as it will power all systems simultaneously.
 * 
 * The following systems are enabled:
 * - All power rails (3.3V, 5V, 12V)
 * - Battery control
 * - Both H-bridge controllers
 * - Both burn wire systems
 */
void enableAllGPIOs(void)
{
    BURN_5V_Set();
    SW_12V_EN1_Set();
    SW_3V3_EN1_Set();
    SW_3V3_EN2_Set();
    SW_5V_EN1_Set();
    SW_5V_EN2_Set();
    SW_5V_EN3_Set();
    SW_5V_EN4_Set();
    SW_VBATT_EN_Set();
    IN1_Set();
    IN2_Set();
    IN3_Set();
    IN4_Set();
    IN5_Set();
    IN6_Set();
    IN7_Set();
    IN8_Set();
    TRQ1_Set();
    TRQ2_Set();
    FAULT1_Set();
    FAULT2_Set();
    SLEEP1_Set();
    SLEEP2_Set();
    BURN1_EN_Set();
    BURN2_EN_Set();
}

/**
 * @brief Disables all PDU GPIO outputs
 * 
 * This function sets all power switches and control interfaces to their inactive state.
 * This is a safe state that powers down all systems.
 * 
 * The following systems are disabled:
 * - All power rails (3.3V, 5V, 12V)
 * - Battery control
 * - Both H-bridge controllers
 * - Both burn wire systems
 */
void disableAllGPIOs(void)
{
    SW_12V_EN1_Clear();
    SW_3V3_EN2_Clear();
    SW_3V3_EN1_Clear();
    SW_5V_EN2_Clear();
    SW_5V_EN1_Clear();
    SW_5V_EN3_Clear();
    SW_5V_EN4_Clear();
    SW_VBATT_EN_Clear();
    IN1_Clear();
    IN2_Clear();
    IN3_Clear();
    IN4_Clear();
    IN5_Clear();
    IN6_Clear();
    IN7_Clear();
    IN8_Clear();
    TRQ1_Clear();
    TRQ2_Clear();
    FAULT1_Clear();
    FAULT2_Clear();
    SLEEP1_Clear();
    SLEEP2_Clear();
    BURN_5V_Clear();
    BURN1_EN_Clear();
    BURN2_EN_Clear();
}