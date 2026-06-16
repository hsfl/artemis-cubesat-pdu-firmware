#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "pdu_packet.h"
#include "pdu_protocol_v2.h"
#include "definitions.h"
#include "version.h"
#include "FreeRTOS.h"
#include "task.h"

#define PDU_PARSER_INTER_BYTE_TIMEOUT_TICKS pdMS_TO_TICKS(100U)

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
    TickType_t lastByteTick;
    uint8_t frame_buf[PDU_V2_MAX_FRAME_LEN];
} PDU_ParserState;

typedef enum
{
    PDU_TIMED_OP_NONE = 0U,
    PDU_TIMED_OP_POWER_CYCLE_RESTORE,
    PDU_TIMED_OP_BURN_FORCE_OFF,
    PDU_TIMED_OP_TORQUE_COAST
} PDU_TimedOpType;

typedef struct
{
    bool active;
    PDU_TimedOpType type;
    uint8_t outputId;
    TickType_t startTick;
    TickType_t delayTicks;
} PDU_TimedOperation;

static PDU_ParserState parserState = {0};
static PDU_TimedOperation timedOperation = {0};
static volatile bool softwareResetRequested = false;

/* FreeRTOS ticks are 16-bit in this project, so keep telemetry uptime wider. */
static bool uptimeInitialized = false;
static TickType_t uptimeLastTick = 0U;
static uint32_t uptimeRemainderTicks = 0U;
static uint32_t uptimeSeconds = 0U;

/*
 * Output arrays and bitmaps use one consistent order everywhere:
 * - GET_SUMMARY_STATUS bitmap
 * - GET_OUTPUT_STATE(all) payload
 *
 * That consistency keeps the controller-side decoding simple.
 */
static const uint8_t s_outputOrder[PDU_V2_OUTPUT_COUNT] = {
    PDU_OUTPUT_3V3_1,
    PDU_OUTPUT_3V3_2,
    PDU_OUTPUT_5V_1,
    PDU_OUTPUT_5V_2,
    PDU_OUTPUT_5V_3,
    PDU_OUTPUT_12V,
    PDU_OUTPUT_VBATT,
    PDU_OUTPUT_BURN1,
    PDU_OUTPUT_BURN2};

static void pdu_parser_reset(void);
static uint16_t pdu_crc16_ccitt(const uint8_t *data, size_t len);
static void pdu_send_bytes(const void *buffer, size_t size);
static bool pdu_is_valid_output_id(uint8_t outputId);
static bool pdu_is_power_cycle_allowed(uint8_t outputId);
static bool pdu_is_burn_output(uint8_t outputId);
static bool pdu_is_timed_operation_active(void);
static void pdu_clear_timed_operation(void);
static void pdu_start_timed_operation(PDU_TimedOpType type, uint8_t outputId, uint16_t delayMs);
static bool pdu_is_valid_torque_coil(uint8_t coilId);
static bool pdu_is_valid_torque_mode(uint8_t mode);
static bool pdu_is_valid_torque_current(uint8_t current);
static uint8_t pdu_get_torque_driver(uint8_t coilId);
static bool pdu_is_torque_coil_active(uint8_t coilId);
static uint8_t pdu_get_torque_coil_mode(uint8_t coilId);
static uint8_t pdu_get_torque_current(uint8_t driver);
static uint8_t pdu_get_torque_fault(uint8_t coilId);
static bool pdu_set_torque_coil(uint8_t coilId, uint8_t mode, uint8_t current);
static void pdu_fill_torque_response(uint8_t coilId, uint8_t *responsePayload);
static void pdu_set_output_state(uint8_t outputId, uint8_t enabled);
static uint8_t pdu_get_output_state(uint8_t outputId);
static void pdu_fill_all_output_states(uint8_t *states);
static uint16_t pdu_get_output_bitmap(void);
static uint8_t pdu_get_fault_bitmap(void);
static uint8_t pdu_get_reset_cause(void);
static uint8_t pdu_get_charger_enabled(void);
static uint8_t pdu_get_charge_indicator_active(void);
static void pdu_set_charger_enabled(uint8_t enabled);
static void pdu_fill_charger_response(uint8_t *responsePayload);
static void pdu_update_uptime(void);
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
    parserState.lastByteTick = 0U;
}

void pdu_protocol_reset_parser(void)
{
    pdu_parser_reset();
}

void pdu_request_software_reset(void)
{
    softwareResetRequested = true;
}

bool pdu_software_reset_requested(void)
{
    return softwareResetRequested;
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

static bool pdu_is_power_cycle_allowed(uint8_t outputId)
{
    switch (outputId)
    {
    case PDU_OUTPUT_3V3_1:
    case PDU_OUTPUT_3V3_2:
    case PDU_OUTPUT_5V_1:
    case PDU_OUTPUT_5V_2:
    case PDU_OUTPUT_5V_3:
    case PDU_OUTPUT_12V:
    case PDU_OUTPUT_VBATT:
        return true;
    default:
        return false;
    }
}

static bool pdu_is_burn_output(uint8_t outputId)
{
    return (outputId == PDU_OUTPUT_BURN1) || (outputId == PDU_OUTPUT_BURN2);
}

static bool pdu_is_timed_operation_active(void)
{
    return timedOperation.active;
}

static void pdu_clear_timed_operation(void)
{
    timedOperation.active = false;
    timedOperation.type = PDU_TIMED_OP_NONE;
    timedOperation.outputId = 0U;
    timedOperation.startTick = 0U;
    timedOperation.delayTicks = 0U;
}

static void pdu_start_timed_operation(PDU_TimedOpType type, uint8_t outputId, uint16_t delayMs)
{
    TickType_t delayTicks = pdMS_TO_TICKS(delayMs);

    if (delayTicks == 0U)
    {
        delayTicks = 1U;
    }

    timedOperation.active = true;
    timedOperation.type = type;
    timedOperation.outputId = outputId;
    timedOperation.startTick = xTaskGetTickCount();
    timedOperation.delayTicks = delayTicks;
}

void pdu_protocol_service_timers(void)
{
    pdu_update_uptime();

    if (!timedOperation.active)
    {
        return;
    }

    if ((TickType_t)(xTaskGetTickCount() - timedOperation.startTick) < timedOperation.delayTicks)
    {
        return;
    }

    switch (timedOperation.type)
    {
    case PDU_TIMED_OP_POWER_CYCLE_RESTORE:
        pdu_set_output_state(timedOperation.outputId, 1U);
        break;
    case PDU_TIMED_OP_BURN_FORCE_OFF:
        pdu_set_output_state(timedOperation.outputId, 0U);
        break;
    case PDU_TIMED_OP_TORQUE_COAST:
        (void)pdu_set_torque_coil(timedOperation.outputId, PDU_TORQUE_MODE_COAST, PDU_TORQUE_CURRENT_100);
        break;
    default:
        break;
    }

    pdu_clear_timed_operation();
}

static bool pdu_is_valid_torque_coil(uint8_t coilId)
{
    return (coilId >= PDU_TORQUE_COIL_1) && (coilId <= PDU_TORQUE_COIL_4);
}

static bool pdu_is_valid_torque_mode(uint8_t mode)
{
    return mode <= PDU_TORQUE_MODE_BRAKE;
}

static bool pdu_is_valid_torque_current(uint8_t current)
{
    return (current == PDU_TORQUE_CURRENT_100) || (current == PDU_TORQUE_CURRENT_50);
}

static uint8_t pdu_get_torque_driver(uint8_t coilId)
{
    return (coilId <= PDU_TORQUE_COIL_2) ? 1U : 2U;
}

static void pdu_set_coil_pair(uint8_t coilId, bool inAHigh, bool inBHigh)
{
    switch (coilId)
    {
    case PDU_TORQUE_COIL_1:
        if (inAHigh)
            IN1_Set();
        else
            IN1_Clear();

        if (inBHigh)
            IN2_Set();
        else
            IN2_Clear();
        break;
    case PDU_TORQUE_COIL_2:
        if (inAHigh)
            IN3_Set();
        else
            IN3_Clear();

        if (inBHigh)
            IN4_Set();
        else
            IN4_Clear();
        break;
    case PDU_TORQUE_COIL_3:
        if (inAHigh)
            IN5_Set();
        else
            IN5_Clear();

        if (inBHigh)
            IN6_Set();
        else
            IN6_Clear();
        break;
    case PDU_TORQUE_COIL_4:
        if (inAHigh)
            IN7_Set();
        else
            IN7_Clear();

        if (inBHigh)
            IN8_Set();
        else
            IN8_Clear();
        break;
    default:
        break;
    }
}

static uint8_t pdu_get_torque_coil_mode(uint8_t coilId)
{
    bool inAHigh = false;
    bool inBHigh = false;

    switch (coilId)
    {
    case PDU_TORQUE_COIL_1:
        inAHigh = PORT_PinRead(IN1_PIN);
        inBHigh = PORT_PinRead(IN2_PIN);
        break;
    case PDU_TORQUE_COIL_2:
        inAHigh = PORT_PinRead(IN3_PIN);
        inBHigh = PORT_PinRead(IN4_PIN);
        break;
    case PDU_TORQUE_COIL_3:
        inAHigh = PORT_PinRead(IN5_PIN);
        inBHigh = PORT_PinRead(IN6_PIN);
        break;
    case PDU_TORQUE_COIL_4:
        inAHigh = PORT_PinRead(IN7_PIN);
        inBHigh = PORT_PinRead(IN8_PIN);
        break;
    default:
        return PDU_TORQUE_MODE_COAST;
    }

    if (!inAHigh && !inBHigh)
    {
        return PDU_TORQUE_MODE_COAST;
    }
    if (inAHigh && !inBHigh)
    {
        return PDU_TORQUE_MODE_FORWARD;
    }
    if (!inAHigh && inBHigh)
    {
        return PDU_TORQUE_MODE_REVERSE;
    }

    return PDU_TORQUE_MODE_BRAKE;
}

static bool pdu_is_torque_coil_active(uint8_t coilId)
{
    return pdu_get_torque_coil_mode(coilId) != PDU_TORQUE_MODE_COAST;
}

static uint8_t pdu_get_torque_current(uint8_t driver)
{
    if (driver == 1U)
    {
        return PORT_PinRead(TRQ1_PIN) ? PDU_TORQUE_CURRENT_50 : PDU_TORQUE_CURRENT_100;
    }

    return PORT_PinRead(TRQ2_PIN) ? PDU_TORQUE_CURRENT_50 : PDU_TORQUE_CURRENT_100;
}

static uint8_t pdu_get_torque_fault(uint8_t coilId)
{
    if (pdu_get_torque_driver(coilId) == 1U)
    {
        return (uint8_t)!PORT_PinRead(FAULT1_PIN);
    }

    return (uint8_t)!PORT_PinRead(FAULT2_PIN);
}

static bool pdu_set_torque_coil(uint8_t coilId, uint8_t mode, uint8_t current)
{
    uint8_t driver = pdu_get_torque_driver(coilId);
    uint8_t sibling = 0U;

    if (!pdu_is_valid_torque_coil(coilId) ||
        !pdu_is_valid_torque_mode(mode) ||
        !pdu_is_valid_torque_current(current))
    {
        return false;
    }

    sibling = (coilId == PDU_TORQUE_COIL_1) ? PDU_TORQUE_COIL_2 :
              (coilId == PDU_TORQUE_COIL_2) ? PDU_TORQUE_COIL_1 :
              (coilId == PDU_TORQUE_COIL_3) ? PDU_TORQUE_COIL_4 :
                                               PDU_TORQUE_COIL_3;

    if ((mode != PDU_TORQUE_MODE_COAST) &&
        pdu_is_torque_coil_active(sibling) &&
        (pdu_get_torque_current(driver) != current))
    {
        return false;
    }

    if (mode != PDU_TORQUE_MODE_COAST)
    {
        if (driver == 1U)
        {
            if (current == PDU_TORQUE_CURRENT_50)
                TRQ1_Set();
            else
                TRQ1_Clear();

            SLEEP1_Set();
        }
        else
        {
            if (current == PDU_TORQUE_CURRENT_50)
                TRQ2_Set();
            else
                TRQ2_Clear();

            SLEEP2_Set();
        }
    }

    switch (mode)
    {
    case PDU_TORQUE_MODE_COAST:
        pdu_set_coil_pair(coilId, false, false);
        break;
    case PDU_TORQUE_MODE_FORWARD:
        pdu_set_coil_pair(coilId, true, false);
        break;
    case PDU_TORQUE_MODE_REVERSE:
        pdu_set_coil_pair(coilId, false, true);
        break;
    case PDU_TORQUE_MODE_BRAKE:
        pdu_set_coil_pair(coilId, true, true);
        break;
    default:
        return false;
    }

    if ((mode == PDU_TORQUE_MODE_COAST) && !pdu_is_torque_coil_active(sibling))
    {
        if (driver == 1U)
        {
            SLEEP1_Clear();
        }
        else
        {
            SLEEP2_Clear();
        }
    }

    return true;
}

static void pdu_fill_torque_response(uint8_t coilId, uint8_t *responsePayload)
{
    uint8_t driver = pdu_get_torque_driver(coilId);

    responsePayload[0] = coilId;
    responsePayload[1] = pdu_get_torque_coil_mode(coilId);
    responsePayload[2] = pdu_get_torque_current(driver);
    responsePayload[3] = (driver == 1U) ? (uint8_t)PORT_PinRead(SLEEP1_PIN) : (uint8_t)PORT_PinRead(SLEEP2_PIN);
    responsePayload[4] = pdu_get_torque_fault(coilId);
}

static void pdu_set_output_state(uint8_t outputId, uint8_t enabled)
{
    /* "All" is read-only in the protocol. Avoid a one-command enable-all path. */
    if (outputId == PDU_V2_OUTPUT_ALL)
    {
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
        case PDU_OUTPUT_12V:
            /* SW_5V_EN4 is the upstream input for the 12 V regulator. */
            SW_5V_EN4_Set();
            SW_12V_EN1_Set();
            break;
        case PDU_OUTPUT_VBATT:
            SW_VBATT_EN_Set();
            break;
        case PDU_OUTPUT_BURN1:
            /* Source first, then channel gate. Fire commands bound the duration. */
            BURN_5V_Set();
            BURN1_EN_Set();
            break;
        case PDU_OUTPUT_BURN2:
            BURN_5V_Set();
            BURN2_EN_Set();
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
        case PDU_OUTPUT_12V:
            SW_12V_EN1_Clear();
            SW_5V_EN4_Clear();
            break;
        case PDU_OUTPUT_VBATT:
            SW_VBATT_EN_Clear();
            break;
        case PDU_OUTPUT_BURN1:
            BURN1_EN_Clear();
            /* Only drop the shared 5 V source if the other burn channel is also off. */
            if (!PORT_PinRead(BURN2_EN_PIN))
            {
                BURN_5V_Clear();
            }
            break;
        case PDU_OUTPUT_BURN2:
            BURN2_EN_Clear();
            if (!PORT_PinRead(BURN1_EN_PIN))
            {
                BURN_5V_Clear();
            }
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
    case PDU_OUTPUT_12V:
        return (uint8_t)(PORT_PinRead(SW_12V_EN1_PIN) && PORT_PinRead(SW_5V_EN4_PIN));
    case PDU_OUTPUT_VBATT:
        return (uint8_t)PORT_PinRead(SW_VBATT_EN_PIN);
    case PDU_OUTPUT_BURN1:
        return (uint8_t)(PORT_PinRead(BURN1_EN_PIN) && PORT_PinRead(BURN_5V_PIN));
    case PDU_OUTPUT_BURN2:
        return (uint8_t)(PORT_PinRead(BURN2_EN_PIN) && PORT_PinRead(BURN_5V_PIN));
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

static uint8_t pdu_get_fault_bitmap(void)
{
    uint8_t faultBitmap = 0U;

    /*
     * DRV8847 nFAULT-style outputs are treated as active-low board fault
     * indications. The pins are not driven by firmware.
     */
    if (!PORT_PinRead(FAULT1_PIN))
    {
        faultBitmap |= PDU_V2_FAULT_BITMAP_HBRIDGE1;
    }

    if (!PORT_PinRead(FAULT2_PIN))
    {
        faultBitmap |= PDU_V2_FAULT_BITMAP_HBRIDGE2;
    }

    return faultBitmap;
}

static uint8_t pdu_get_reset_cause(void)
{
    return RSTC_REGS->RSTC_RCAUSE;
}

static uint8_t pdu_get_charger_enabled(void)
{
    return PORT_PinLatchRead(SHDN_PIN) ? 1U : 0U;
}

static uint8_t pdu_get_charge_indicator_active(void)
{
    return PORT_PinRead(CHRG_PIN) ? 0U : 1U;
}

static void pdu_set_charger_enabled(uint8_t enabled)
{
    if (enabled != 0U)
    {
        SHDN_Set();
    }
    else
    {
        SHDN_Clear();
    }
}

static void pdu_fill_charger_response(uint8_t *responsePayload)
{
    responsePayload[0] = pdu_get_charger_enabled();
    responsePayload[1] = pdu_get_charge_indicator_active();
    responsePayload[2] = (uint8_t)PORT_PinLatchRead(SHDN_PIN);
    responsePayload[3] = (uint8_t)PORT_PinRead(CHRG_PIN);
}

static void pdu_update_uptime(void)
{
    TickType_t currentTick = xTaskGetTickCount();
    TickType_t elapsedTicks;
    uint32_t totalTicks;

    if (!uptimeInitialized)
    {
        uptimeLastTick = currentTick;
        uptimeInitialized = true;
        return;
    }

    elapsedTicks = (TickType_t)(currentTick - uptimeLastTick);
    if (elapsedTicks == 0U)
    {
        return;
    }

    uptimeLastTick = currentTick;
    totalTicks = uptimeRemainderTicks + (uint32_t)elapsedTicks;
    uptimeSeconds += totalTicks / (uint32_t)configTICK_RATE_HZ;
    uptimeRemainderTicks = totalTicks % (uint32_t)configTICK_RATE_HZ;
}

static uint32_t pdu_get_uptime_seconds(void)
{
    pdu_update_uptime();
    return uptimeSeconds;
}

static uint8_t pdu_get_capabilities(void)
{
    return PDU_V2_CAP_CRC16 |
           PDU_V2_CAP_SEQUENCE_ACK |
           PDU_V2_CAP_RESET_INFO |
           PDU_V2_CAP_SUMMARY_STATUS |
           PDU_V2_CAP_POWER_CYCLE |
           PDU_V2_CAP_SOFTWARE_RESET |
           PDU_V2_CAP_HELP |
           PDU_V2_CAP_ACTUATOR_COMMANDS;
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
    case PDU_V2_OP_PING:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        responsePayload[0] = PDU_V2_VERSION;
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_PING_LEN);
        return;

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
            uint32_t currentUptimeSeconds = pdu_get_uptime_seconds();

            responsePayload[0] = (uint8_t)(outputBitmap & 0xFFU);
            responsePayload[1] = (uint8_t)(outputBitmap >> 8U);
            responsePayload[2] = pdu_get_reset_cause();
            responsePayload[3] = pdu_get_fault_bitmap();
            responsePayload[4] = (uint8_t)(currentUptimeSeconds & 0xFFU);
            responsePayload[5] = (uint8_t)((currentUptimeSeconds >> 8U) & 0xFFU);
            responsePayload[6] = (uint8_t)((currentUptimeSeconds >> 16U) & 0xFFU);
            responsePayload[7] = (uint8_t)((currentUptimeSeconds >> 24U) & 0xFFU);
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

    case PDU_V2_OP_HELP:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        {
            static const char helpPayload[] =
                VERSION_STRING " cmds:PING,INFO,SUM,RI,HELP,GET,SET,CYC,BURN,TRQ,TRQ?,CHG,CHG?,RST";
            pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, (const uint8_t *)helpPayload, (uint8_t)(sizeof(helpPayload) - 1U));
        }
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

        if (payload[0] == PDU_V2_OUTPUT_ALL)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        if (!pdu_is_valid_output_id(payload[0]))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        if ((payload[1] != 0U) && (payload[1] != 1U))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        if (pdu_is_burn_output(payload[0]))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        if (pdu_is_timed_operation_active())
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BUSY, NULL, 0U);
            return;
        }

        pdu_set_output_state(payload[0], payload[1]);

        responsePayload[0] = payload[0];
        responsePayload[1] = pdu_get_output_state(payload[0]);
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, 2U);
        return;

    case PDU_V2_OP_POWER_CYCLE_OUTPUT:
        if (payload_len != PDU_V2_POWER_CYCLE_REQ_LEN)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        {
            uint8_t outputId = payload[0];
            uint16_t offTimeMs = (uint16_t)payload[1] | ((uint16_t)payload[2] << 8U);
            uint8_t initialState;

            if (!pdu_is_power_cycle_allowed(outputId) ||
                (offTimeMs == 0U) ||
                (offTimeMs > PDU_V2_POWER_CYCLE_MAX_MS))
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
                return;
            }

            if (pdu_is_timed_operation_active())
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BUSY, NULL, 0U);
                return;
            }

            initialState = pdu_get_output_state(outputId);
            if (initialState == 0U)
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
                return;
            }

            pdu_set_output_state(outputId, 0U);
            pdu_start_timed_operation(PDU_TIMED_OP_POWER_CYCLE_RESTORE, outputId, offTimeMs);

            responsePayload[0] = outputId;
            responsePayload[1] = pdu_get_output_state(outputId);
            pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, 2U);
        }
        return;

    case PDU_V2_OP_FIRE_BURN_WIRE:
        if (payload_len != PDU_V2_FIRE_BURN_REQ_LEN)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        {
            uint8_t outputId = payload[0];
            uint16_t fireTimeMs = (uint16_t)payload[1] | ((uint16_t)payload[2] << 8U);
            uint16_t armToken = (uint16_t)payload[3] | ((uint16_t)payload[4] << 8U);

            if (!pdu_is_burn_output(outputId) ||
                (fireTimeMs == 0U) ||
                (fireTimeMs > PDU_V2_FIRE_BURN_MAX_MS) ||
                (armToken != PDU_V2_BURN_ARM_TOKEN))
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
                return;
            }

            if (pdu_is_timed_operation_active() ||
                (pdu_get_output_state(PDU_OUTPUT_BURN1) != 0U) ||
                (pdu_get_output_state(PDU_OUTPUT_BURN2) != 0U))
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BUSY, NULL, 0U);
                return;
            }

            pdu_set_output_state(outputId, 1U);
            pdu_start_timed_operation(PDU_TIMED_OP_BURN_FORCE_OFF, outputId, fireTimeMs);

            responsePayload[0] = outputId;
            responsePayload[1] = pdu_get_output_state(outputId);
            pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, 2U);
        }
        return;

    case PDU_V2_OP_SET_TORQUE_COIL:
        if (payload_len != PDU_V2_SET_TORQUE_COIL_REQ_LEN)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        {
            uint8_t coilId = payload[0];
            uint8_t mode = payload[1];
            uint8_t current = payload[2];
            uint16_t durationMs = (uint16_t)payload[3] | ((uint16_t)payload[4] << 8U);

            if (!pdu_is_valid_torque_coil(coilId) ||
                !pdu_is_valid_torque_mode(mode) ||
                !pdu_is_valid_torque_current(current) ||
                (durationMs > PDU_V2_TORQUE_COIL_MAX_MS))
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
                return;
            }

            if (timedOperation.active &&
                ((timedOperation.type != PDU_TIMED_OP_TORQUE_COAST) ||
                 ((timedOperation.outputId != coilId) && (durationMs > 0U))))
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BUSY, NULL, 0U);
                return;
            }

            if (timedOperation.active &&
                (timedOperation.type == PDU_TIMED_OP_TORQUE_COAST) &&
                (timedOperation.outputId == coilId))
            {
                pdu_clear_timed_operation();
            }

            if (!pdu_set_torque_coil(coilId, mode, current))
            {
                pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
                return;
            }

            if ((durationMs > 0U) && (mode != PDU_TORQUE_MODE_COAST))
            {
                pdu_start_timed_operation(PDU_TIMED_OP_TORQUE_COAST, coilId, durationMs);
            }

            pdu_fill_torque_response(coilId, responsePayload);
            pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_TORQUE_COIL_RESP_LEN);
        }
        return;

    case PDU_V2_OP_GET_TORQUE_COIL:
        if (payload_len != PDU_V2_GET_TORQUE_COIL_REQ_LEN)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        if (!pdu_is_valid_torque_coil(payload[0]))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        pdu_fill_torque_response(payload[0], responsePayload);
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_TORQUE_COIL_RESP_LEN);
        return;

    case PDU_V2_OP_GET_CHARGER_STATUS:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        pdu_fill_charger_response(responsePayload);
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_CHARGER_STATUS_RESP_LEN);
        return;

    case PDU_V2_OP_SET_CHARGER_STATE:
        if (payload_len != PDU_V2_SET_CHARGER_STATE_REQ_LEN)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        if ((payload[0] != 0U) && (payload[0] != 1U))
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_PARAM, NULL, 0U);
            return;
        }

        pdu_set_charger_enabled(payload[0]);
        pdu_fill_charger_response(responsePayload);
        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, responsePayload, PDU_V2_CHARGER_STATUS_RESP_LEN);
        return;

    case PDU_V2_OP_SOFTWARE_RESET:
        if (payload_len != 0U)
        {
            pdu_send_response(seq, opcode, PDU_V2_STATUS_BAD_LENGTH, NULL, 0U);
            return;
        }

        pdu_send_response(seq, opcode, PDU_V2_STATUS_OK, NULL, 0U);
        pdu_request_software_reset();
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
    TickType_t currentTick = xTaskGetTickCount();

    if (parserState.in_frame &&
        ((TickType_t)(currentTick - parserState.lastByteTick) > PDU_PARSER_INTER_BYTE_TIMEOUT_TICKS))
    {
        pdu_parser_reset();
    }

    if (parserState.in_frame)
    {
        /*
         * Once a frame starts, the explicit length field drives collection.
         * This allows 0xA5 to appear as normal data inside the payload or CRC.
         */
        if (parserState.frame_len >= PDU_V2_MAX_FRAME_LEN)
        {
            pdu_parser_reset();
            return;
        }

        parserState.lastByteTick = currentTick;
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
        parserState.lastByteTick = currentTick;
    }
}

void disableAllGPIOs(void)
{
    SHDN_Clear();
    CHRG_InputEnable();
    BURN2_EN_Clear();
    BURN1_EN_Clear();
    BURN_5V_Clear();
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
    SLEEP1_Clear();
    SLEEP2_Clear();
    FAULT1_InputEnable();
    FAULT2_InputEnable();
}
