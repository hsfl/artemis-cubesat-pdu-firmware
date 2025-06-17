#include "pdu_packet.h"
#include "definitions.h"

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
                SW_12V_EN1_Set();
                SW_5V_EN4_Set();
                break;
            case PDU_SW_VBATT:
                SW_VBATT_EN_Set();
                break;
            case PDU_SW_HBRIDGE1:
                FAULT1_Set();
                IN1_Set();
                IN2_Set();
                IN3_Set();
                IN4_Set();
                TRQ1_Set();
                SLEEP1_Set();
                break;
            case PDU_SW_HBRIDGE2:
                FAULT2_Set();
                IN5_Set();
                IN6_Set();
                IN7_Set();
                IN8_Set();
                TRQ2_Set();
                SLEEP2_Set();
                break;
            case PDU_SW_BURN:
                BURN1_EN_Set();
                BURN2_EN_Set();
                BURN_5V_Set();
                break;
            case PDU_SW_BURN1:
                BURN1_EN_Set();
                BURN_5V_Set();
                break;
            case PDU_SW_BURN2:
                BURN2_EN_Set();
                BURN_5V_Set();
                break;
            default:
                break;
            }
        }
        else
        {
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
                SW_12V_EN1_Clear();
                SW_5V_EN4_Clear();
                break;
            case PDU_SW_VBATT:
                SW_VBATT_EN_Clear();
                break;
            case PDU_SW_HBRIDGE1:
                FAULT1_Clear();
                IN1_Clear();
                IN2_Clear();
                IN3_Clear();
                IN4_Clear();
                TRQ1_Clear();
                SLEEP1_Clear();
                break;
            case PDU_SW_HBRIDGE2:
                FAULT2_Clear();
                IN5_Clear();
                IN6_Clear();
                IN7_Clear();
                IN8_Clear();
                TRQ2_Clear();
                SLEEP2_Clear();
                break;
            case PDU_SW_BURN:
                BURN1_EN_Clear();
                BURN2_EN_Clear();
                BURN_5V_Clear();
                break;
            case PDU_SW_BURN1:
                if (!PORT_PinRead(BURN2_EN_PIN))
                    BURN_5V_Clear();
                BURN1_EN_Clear();
                break;
            case PDU_SW_BURN2:
                if (!PORT_PinRead(BURN1_EN_PIN))
                    BURN_5V_Clear();
                BURN2_EN_Clear();
                break;
            default:
                break;
            }
        }
        break; // Prevent fall-through to CommandGetSwitchStatus
    case PDU_TYPE_COMMAND_GET_SWITCH_STATUS:
        if (packet.sw == PDU_SW_ALL)
        {
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
            packet.sw_state = PORT_PinRead(SW_12V_EN1_PIN) && PORT_PinRead(SW_5V_EN4_PIN);
            break;
        case PDU_SW_VBATT:
            packet.sw_state = PORT_PinRead(SW_VBATT_EN_PIN);
            break;
        case PDU_SW_HBRIDGE1:
            packet.sw_state = PORT_PinRead(FAULT1_PIN) &&
                              PORT_PinRead(IN1_PIN) &&
                              PORT_PinRead(IN2_PIN) &&
                              PORT_PinRead(IN3_PIN) &&
                              PORT_PinRead(IN4_PIN) &&
                              PORT_PinRead(TRQ1_PIN) &&
                              PORT_PinRead(SLEEP1_PIN);
            break;
        case PDU_SW_HBRIDGE2:
            packet.sw_state = PORT_PinRead(FAULT2_PIN) &&
                              PORT_PinRead(IN5_PIN) &&
                              PORT_PinRead(IN6_PIN) &&
                              PORT_PinRead(IN7_PIN) &&
                              PORT_PinRead(IN8_PIN) &&
                              PORT_PinRead(TRQ2_PIN) &&
                              PORT_PinRead(SLEEP2_PIN);
            break;
        case PDU_SW_BURN:
            packet.sw_state = PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN2_EN_PIN) && PORT_PinRead(BURN_5V_PIN);
            break;
        case PDU_SW_BURN1:
            packet.sw_state = PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN_5V_PIN);
            break;
        case PDU_SW_BURN2:
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