#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pdu_packet.h"
#include "pdu_protocol_v2.h"
#include "definitions.h"
#include "version.h"

/*
 * The parser keeps only the minimum state needed to assemble one framed UART
 * message at a time. Keeping this structure small makes it easier to reason
 * about recovery after a corrupted or truncated frame.
 */
typedef struct
{
    bool in_frame;
    uint8_t frame_len;
    uint8_t expected_len;
    uint8_t frame_buf[PDU_V2_MAX_FRAME_LEN];
} PDU_ParserState;

static PDU_ParserState parserState = {0};

/*
 * Output arrays and bitmaps use one consistent order everywhere:
 * - GET_SUMMARY_STATUS bitmap
 * - GET_OUTPUT_STATE(all) payload
 * - SET_OUTPUT_STATE(all) response payload
 *
 * That consistency keeps the controller-side decoding simple.
 */
static const uint8_t s_outputOrder[PDU_V2_OUTPUT_COUNT] = {
    PDU_OUTPUT_3V3_1,
    PDU_OUTPUT_3V3_2,
    PDU_OUTPUT_5V_1,
    PDU_OUTPUT_5V_2,
    PDU_OUTPUT_5V_3,
    PDU_OUTPUT_5V_4,
    PDU_OUTPUT_12V,
    PDU_OUTPUT_VBATT,
    PDU_OUTPUT_BURN1,
    PDU_OUTPUT_BURN2,
    PDU_OUTPUT_HBRIDGE1,
    PDU_OUTPUT_HBRIDGE2};

static void enableAllGPIOs(void);
static void pdu_parser_reset(void);
static uint16_t pdu_crc16_ccitt(const uint8_t *data, size_t len);
static void pdu_send_bytes(const void *buffer, size_t size);
static bool pdu_is_valid_output_id(uint8_t outputId);
static void pdu_set_output_state(uint8_t outputId, uint8_t enabled);
static uint8_t pdu_get_output_state(uint8_t outputId);
static void pdu_fill_all_output_states(uint8_t *states);
static uint16_t pdu_get_output_bitmap(void);
static uint8_t pdu_get_reset_cause(void);
static uint32_t pdu_get_uptime_seconds(void);
static uint8_t pdu_get_capabilities(void);
static void pdu_send_response(uint8_t seq, uint8_t opcode, uint8_t status, const uint8_t *payload, uint8_t payload_len);
static void pdu_handle_request(uint8_t opcode, uint8_t seq, const uint8_t *payload, uint8_t payload_len);
static void pdu_handle_complete_frame(const uint8_t *frame, uint8_t frame_len);

static void pdu_parser_reset(void)
{
    parserState.in_frame = false;
    parserState.frame_len = 0U;
    parserState.expected_len = 0U;
}

static uint16_t pdu_crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    /*
     * CRC-16/CCITT is a lightweight error-detection checksum commonly used on
     * embedded links. Here it covers the entire frame except the SOF byte.
     * If the sender and receiver compute different CRC values, the frame is
     * treated as corrupted and ignored.
     */
    for (size_t index = 0U; index < len; index++)
    {
        crc ^= (uint16_t)data[index] << 8;

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc <<= 1U;
            }
        }
    }

    return crc;
}

static void pdu_send_bytes(const void *buffer, size_t size)
{
    (void)SERCOM3_USART_Write((void *)buffer, size);
}

static bool pdu_is_valid_output_id(uint8_t outputId)
{
    for (uint8_t index = 0U; index < PDU_V2_OUTPUT_COUNT; index++)
    {
        if (s_outputOrder[index] == outputId)
        {
            return true;
        }
    }

    return false;
}

static void pdu_set_output_state(uint8_t outputId, uint8_t enabled)
{
    /* "All" is treated as a convenience command for bring-up and bench testing. */
    if (outputId == PDU_V2_OUTPUT_ALL)
    {
        if (enabled != 0U)
        {
            enableAllGPIOs();
        }
        else
        {
            disableAllGPIOs();
        }

        return;
    }

    if (enabled != 0U)
    {
        switch (outputId)
        {
        case PDU_OUTPUT_3V3_1:
            SW_3V3_EN1_Set();
            break;
        case PDU_OUTPUT_3V3_2:
            SW_3V3_EN2_Set();
            break;
        case PDU_OUTPUT_5V_1:
            SW_5V_EN1_Set();
            break;
        case PDU_OUTPUT_5V_2:
            SW_5V_EN2_Set();
            break;
        case PDU_OUTPUT_5V_3:
            SW_5V_EN3_Set();
            break;
        case PDU_OUTPUT_5V_4:
            SW_5V_EN4_Set();
            break;
        case PDU_OUTPUT_12V:
            /* The 12 V rail depends on its upstream 5 V regulator input. */
            SW_5V_EN4_Set();
            SW_12V_EN1_Set();
            break;
        case PDU_OUTPUT_VBATT:
            SW_VBATT_EN_Set();
            break;
        case PDU_OUTPUT_BURN1:
            /* Burn channels share the 5 V source, so enabling either requires it. */
            BURN1_EN_Set();
            BURN_5V_Set();
            break;
        case PDU_OUTPUT_BURN2:
            BURN2_EN_Set();
            BURN_5V_Set();
            break;
        case PDU_OUTPUT_HBRIDGE1:
            /* H-bridge outputs are logical composites, not single-pin controls. */
            FAULT1_Set();
            IN1_Set();
            IN2_Set();
            IN3_Set();
            IN4_Set();
            TRQ1_Set();
            SLEEP1_Set();
            break;
        case PDU_OUTPUT_HBRIDGE2:
            FAULT2_Set();
            IN5_Set();
            IN6_Set();
            IN7_Set();
            IN8_Set();
            TRQ2_Set();
            SLEEP2_Set();
            break;
        default:
            break;
        }
    }
    else
    {
        switch (outputId)
        {
        case PDU_OUTPUT_3V3_1:
            SW_3V3_EN1_Clear();
            break;
        case PDU_OUTPUT_3V3_2:
            SW_3V3_EN2_Clear();
            break;
        case PDU_OUTPUT_5V_1:
            SW_5V_EN1_Clear();
            break;
        case PDU_OUTPUT_5V_2:
            SW_5V_EN2_Clear();
            break;
        case PDU_OUTPUT_5V_3:
            SW_5V_EN3_Clear();
            break;
        case PDU_OUTPUT_5V_4:
            SW_5V_EN4_Clear();
            break;
        case PDU_OUTPUT_12V:
            SW_12V_EN1_Clear();
            SW_5V_EN4_Clear();
            break;
        case PDU_OUTPUT_VBATT:
            SW_VBATT_EN_Clear();
            break;
        case PDU_OUTPUT_BURN1:
            /* Only drop the shared 5 V source if the other burn channel is also off. */
            if (!PORT_PinRead(BURN2_EN_PIN))
            {
                BURN_5V_Clear();
            }
            BURN1_EN_Clear();
            break;
        case PDU_OUTPUT_BURN2:
            if (!PORT_PinRead(BURN1_EN_PIN))
            {
                BURN_5V_Clear();
            }
            BURN2_EN_Clear();
            break;
        case PDU_OUTPUT_HBRIDGE1:
            FAULT1_Clear();
            IN1_Clear();
            IN2_Clear();
            IN3_Clear();
            IN4_Clear();
            TRQ1_Clear();
            SLEEP1_Clear();
            break;
        case PDU_OUTPUT_HBRIDGE2:
            FAULT2_Clear();
            IN5_Clear();
            IN6_Clear();
            IN7_Clear();
            IN8_Clear();
            TRQ2_Clear();
            SLEEP2_Clear();
            break;
        default:
            break;
        }
    }
}

static uint8_t pdu_get_output_state(uint8_t outputId)
{
    /* These reads report logical output state, including composite outputs. */
    switch (outputId)
    {
    case PDU_OUTPUT_3V3_1:
        return (uint8_t)PORT_PinRead(SW_3V3_EN1_PIN);
    case PDU_OUTPUT_3V3_2:
        return (uint8_t)PORT_PinRead(SW_3V3_EN2_PIN);
    case PDU_OUTPUT_5V_1:
        return (uint8_t)PORT_PinRead(SW_5V_EN1_PIN);
    case PDU_OUTPUT_5V_2:
        return (uint8_t)PORT_PinRead(SW_5V_EN2_PIN);
    case PDU_OUTPUT_5V_3:
        return (uint8_t)PORT_PinRead(SW_5V_EN3_PIN);
    case PDU_OUTPUT_5V_4:
        return (uint8_t)PORT_PinRead(SW_5V_EN4_PIN);
    case PDU_OUTPUT_12V:
        return (uint8_t)(PORT_PinRead(SW_12V_EN1_PIN) && PORT_PinRead(SW_5V_EN4_PIN));
    case PDU_OUTPUT_VBATT:
        return (uint8_t)PORT_PinRead(SW_VBATT_EN_PIN);
    case PDU_OUTPUT_BURN1:
        return (uint8_t)(PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN_5V_PIN));
    case PDU_OUTPUT_BURN2:
        return (uint8_t)(PORT_PinRead(BURN2_EN_PIN) && PORT_PinRead(BURN_5V_PIN));
    case PDU_OUTPUT_HBRIDGE1:
        return (uint8_t)(PORT_PinRead(FAULT1_PIN) &&
                         PORT_PinRead(IN1_PIN) &&
                         PORT_PinRead(IN2_PIN) &&
                         PORT_PinRead(IN3_PIN) &&
                         PORT_PinRead(IN4_PIN) &&
                         PORT_PinRead(TRQ1_PIN) &&
                         PORT_PinRead(SLEEP1_PIN));
    case PDU_OUTPUT_HBRIDGE2:
        return (uint8_t)(PORT_PinRead(FAULT2_PIN) &&
                         PORT_PinRead(IN5_PIN) &&
                         PORT_PinRead(IN6_PIN) &&
                         PORT_PinRead(IN7_PIN) &&
                         PORT_PinRead(IN8_PIN) &&
                         PORT_PinRead(TRQ2_PIN) &&
                         PORT_PinRead(SLEEP2_PIN));
    default:
        return 0U;
    }
}

static void pdu_fill_all_output_states(uint8_t *states)
{
    for (uint8_t index = 0U; index < PDU_V2_OUTPUT_COUNT; index++)
    {
        states[index] = pdu_get_output_state(s_outputOrder[index]);
    }
}

static uint16_t pdu_get_output_bitmap(void)
{
    uint16_t bitmap = 0U;

    /* Reuse the same logical state helper so telemetry matches command semantics. */
    for (uint8_t index = 0U; index < PDU_V2_OUTPUT_COUNT; index++)
    {
        if (pdu_get_output_state(s_outputOrder[index]) != 0U)
        {
            bitmap |= (uint16_t)(1U << index);
        }
    }

    return bitmap;
}

static uint8_t pdu_get_reset_cause(void)
{
    return RSTC_REGS->RSTC_RCAUSE;
}

static uint32_t pdu_get_uptime_seconds(void)
{
    uint32_t rtcFrequency = RTC_Timer32FrequencyGet();

    if (rtcFrequency == 0U)
    {
        return 0U;
    }

    return RTC_Timer32CounterGet() / rtcFrequency;
}

static uint8_t pdu_get_capabilities(void)
{
    return PDU_V2_CAP_CRC16 |
           PDU_V2_CAP_SEQUENCE_ACK |
           PDU_V2_CAP_RESET_INFO |
           PDU_V2_CAP_SUMMARY_STATUS;
}

static void pdu_send_response(uint8_t seq, uint8_t opcode, uint8_t status, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t frame[PDU_V2_MAX_FRAME_LEN] = {0};
    uint16_t crc;
    uint8_t total_len;

    if (payload_len > PDU_V2_MAX_PAYLOAD_LEN)
    {
        return;
    }

    total_len = (uint8_t)(PDU_V2_HEADER_LEN + payload_len + PDU_V2_CRC_LEN);

    /*
     * The response reuses the request opcode and sequence number. That keeps the
     * host protocol simple because it can correlate "this reply belongs to that
     * request" without guessing based on timing.
     */
    frame[0] = PDU_V2_SOF;
    frame[1] = PDU_V2_VERSION;
    frame[2] = PDU_V2_MSG_RESPONSE;
    frame[3] = opcode;
    frame[4] = seq;
    frame[5] = status;
    frame[6] = payload_len;

    if ((payload != NULL) && (payload_len > 0U))
    {
        memcpy(&frame[PDU_V2_HEADER_LEN], payload, payload_len);
    }

    crc = pdu_crc16_ccitt(&frame[1], (size_t)(PDU_V2_HEADER_LEN - 1U + payload_len));
    frame[PDU_V2_HEADER_LEN + payload_len] = (uint8_t)(crc & 0xFFU);
    frame[PDU_V2_HEADER_LEN + payload_len + 1U] = (uint8_t)(crc >> 8U);

    pdu_send_bytes(frame, total_len);
}

static void pdu_handle_request(uint8_t opcode, uint8_t seq, const uint8_t *payload, uint8_t payload_len)
{
    uint8_t responsePayload[PDU_V2_MAX_PAYLOAD_LEN] = {0};

    /* Each case fully validates its payload before touching hardware. */
    switch (opcode)
    {
    case PDU_V2_OP_GET_PROTOCOL_INFO:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        responsePayload[0] = PDU_V2_VERSION;
        responsePayload[1] = pdu_get_capabilities();
        responsePayload[2] = PDU_V2_MAX_PAYLOAD_LEN;
        responsePayload[3] = PDU_V2_OUTPUT_COUNT;
        responsePayload[4] = VERSION_MAJOR;
        responsePayload[5] = VERSION_MINOR;
        responsePayload[6] = VERSION_PATCH;
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_PROTOCOL_INFO_LEN);
        return;

    case PDU_V2_OP_GET_SUMMARY_STATUS:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        {
            uint16_t outputBitmap = pdu_get_output_bitmap();
            uint32_t uptimeSeconds = pdu_get_uptime_seconds();

            responsePayload[0] = (uint8_t)(outputBitmap & 0xFFU);
            responsePayload[1] = (uint8_t)(outputBitmap >> 8U);
            responsePayload[2] = pdu_get_reset_cause();
            responsePayload[3] = PDU_V2_FAULT_BITMAP_NONE;
            responsePayload[4] = (uint8_t)(uptimeSeconds & 0xFFU);
            responsePayload[5] = (uint8_t)((uptimeSeconds >> 8U) & 0xFFU);
            responsePayload[6] = (uint8_t)((uptimeSeconds >> 16U) & 0xFFU);
            responsePayload[7] = (uint8_t)((uptimeSeconds >> 24U) & 0xFFU);
            responsePayload[8] = pdu_get_capabilities();
        }

        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_SUMMARY_STATUS_LEN);
        return;

    case PDU_V2_OP_GET_RESET_INFO:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        responsePayload[0] = pdu_get_reset_cause();
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, 1U);
        return;

    case PDU_V2_OP_GET_OUTPUT_STATE:
        if (payload_len != 1U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        if (payload[0] == PDU_V2_OUTPUT_ALL)
        {
            /* "All" responses use the shared output order table documented in the ICD. */
            responsePayload[0] = PDU_V2_OUTPUT_ALL;
            responsePayload[1] = PDU_V2_OUTPUT_COUNT;
            pdu_fill_all_output_states(&responsePayload[2]);
            pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, (uint8_t)(2U + PDU_V2_OUTPUT_COUNT));
            return;
        }

        if (!pdu_is_valid_output_id(payload[0]))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        responsePayload[0] = payload[0];
        responsePayload[1] = pdu_get_output_state(payload[0]);
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, 2U);
        return;

    case PDU_V2_OP_SET_OUTPUT_STATE:
        if (payload_len != 2U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        if ((payload[0] != PDU_V2_OUTPUT_ALL) && !pdu_is_valid_output_id(payload[0]))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        if ((payload[1] != 0U) && (payload[1] != 1U))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        pdu_set_output_state(payload[0], payload[1]);

        if (payload[0] == PDU_V2_OUTPUT_ALL)
        {
            /* Return post-command state so the caller gets one-shot command acknowledgement. */
            responsePayload[0] = PDU_V2_OUTPUT_ALL;
            responsePayload[1] = PDU_V2_OUTPUT_COUNT;
            pdu_fill_all_output_states(&responsePayload[2]);
            pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, (uint8_t)(2U + PDU_V2_OUTPUT_COUNT));
            return;
        }

        responsePayload[0] = payload[0];
        responsePayload[1] = pdu_get_output_state(payload[0]);
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, 2U);
        return;

    default:
        pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_OPCODE, NULL, 0U);
        return;
    }
}

static void pdu_handle_complete_frame(const uint8_t *frame, uint8_t frame_len)
{
    uint8_t payload_len = frame[6];
    uint16_t frameCrc;
    uint16_t computedCrc;

    if (frame_len != (uint8_t)(PDU_V2_HEADER_LEN + payload_len + PDU_V2_CRC_LEN))
    {
        return;
    }

    frameCrc = (uint16_t)frame[PDU_V2_HEADER_LEN + payload_len] |
               ((uint16_t)frame[PDU_V2_HEADER_LEN + payload_len + 1U] << 8U);
    computedCrc = pdu_crc16_ccitt(&frame[1], (size_t)(PDU_V2_HEADER_LEN - 1U + payload_len));

    /*
     * Bad CRC means the bytes on the wire did not arrive intact. The safest
     * choice is to ignore the frame completely so corrupted traffic can never
     * toggle power outputs by accident.
     */
    if (frameCrc != computedCrc)
    {
        return;
    }

    if (frame[1] != PDU_V2_VERSION)
    {
        pdu_send_response(frame[4], frame[3], PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
        return;
    }

    if (frame[2] != PDU_V2_MSG_REQUEST)
    {
        return;
    }

    pdu_handle_request(frame[3], frame[4], &frame[PDU_V2_HEADER_LEN], payload_len);
}

void pdu_protocol_process_byte(uint8_t byte)
{
    if (parserState.in_frame)
    {
        /*
         * SOF means "start of frame". If we see a fresh SOF while already
         * receiving a frame, we assume the old frame was damaged or truncated
         * and restart parsing from this new byte.
         */
        if (byte == PDU_V2_SOF)
        {
            parserState.frame_buf[0] = byte;
            parserState.frame_len = 1U;
            parserState.expected_len = 0U;
            return;
        }

        if (parserState.frame_len >= PDU_V2_MAX_FRAME_LEN)
        {
            pdu_parser_reset();
            return;
        }

        parserState.frame_buf[parserState.frame_len++] = byte;

        if (parserState.frame_len == PDU_V2_HEADER_LEN)
        {
            uint8_t payload_len = parserState.frame_buf[6];

            if (payload_len > PDU_V2_MAX_PAYLOAD_LEN)
            {
                pdu_parser_reset();
                return;
            }

            parserState.expected_len = (uint8_t)(PDU_V2_HEADER_LEN + payload_len + PDU_V2_CRC_LEN);
        }

        if ((parserState.expected_len != 0U) && (parserState.frame_len == parserState.expected_len))
        {
            pdu_handle_complete_frame(parserState.frame_buf, parserState.frame_len);
            pdu_parser_reset();
        }

        return;
    }

    if (byte == PDU_V2_SOF)
    {
        /* Ignore all traffic until a clean SOF marker is observed. */
        pdu_parser_reset();
        parserState.in_frame = true;
        parserState.frame_buf[0] = byte;
        parserState.frame_len = 1U;
    }
}

static void enableAllGPIOs(void)
{
    BURN_5V_Set();
    BURN2_EN_Set();
    SW_12V_EN1_Set();
    SW_3V3_EN1_Set();
    SW_3V3_EN2_Set();
    SW_5V_EN1_Set();
    SW_5V_EN2_Set();
    SW_5V_EN3_Set();
    SW_5V_EN4_Set();
    SW_VBATT_EN_Set();
    BURN1_EN_Set();
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
}

void disableAllGPIOs(void)
{
    BURN_5V_Clear();
    BURN2_EN_Clear();
    SW_12V_EN1_Clear();
    SW_3V3_EN2_Clear();
    SW_3V3_EN1_Clear();
    SW_5V_EN2_Clear();
    SW_5V_EN1_Clear();
    SW_5V_EN3_Clear();
    SW_5V_EN4_Clear();
    SW_VBATT_EN_Clear();
    BURN1_EN_Set();
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
}
