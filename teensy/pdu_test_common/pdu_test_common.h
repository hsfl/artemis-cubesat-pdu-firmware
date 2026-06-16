/*
  Shared Artemis PDU v2 Teensy test helpers.

  Keep this file small and boring: it is a bench-test UART client for the PDU
  framed binary protocol, not flight software.
*/

#ifndef PDU_TEST_COMMON_H
#define PDU_TEST_COMMON_H

#include <Arduino.h>
#include "pdu_protocol_v2.h"

const uint32_t PDU_TEST_CONSOLE_BAUD = 9600;
const uint32_t PDU_TEST_UART_BAUD = 9600;

const uint8_t PDU_TEST_OUTPUT_ORDER[] = {
  PDU_OUTPUT_3V3_1,
  PDU_OUTPUT_3V3_2,
  PDU_OUTPUT_5V_1,
  PDU_OUTPUT_5V_2,
  PDU_OUTPUT_5V_3,
  PDU_OUTPUT_12V,
  PDU_OUTPUT_VBATT,
  PDU_OUTPUT_BURN1,
  PDU_OUTPUT_BURN2,
};

const size_t PDU_TEST_OUTPUT_COUNT = sizeof(PDU_TEST_OUTPUT_ORDER) / sizeof(PDU_TEST_OUTPUT_ORDER[0]);

struct PduFrame {
  uint8_t version;
  uint8_t msgType;
  uint8_t opcode;
  uint8_t seq;
  uint8_t status;
  uint8_t payloadLen;
  uint8_t payload[PDU_V2_MAX_PAYLOAD_LEN];
};

struct PduProtocolInfo {
  uint8_t protocolVersion;
  uint8_t capabilities;
  uint8_t maxPayload;
  uint8_t outputCount;
  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  uint8_t firmwarePatch;
};

struct PduSummary {
  uint16_t outputBitmap;
  uint8_t resetCause;
  uint8_t faultBitmap;
  uint32_t uptimeSeconds;
  uint8_t capabilities;
};

struct PduOutputSnapshot {
  uint8_t count;
  uint8_t states[PDU_TEST_OUTPUT_COUNT];
};

struct PduTorqueState {
  uint8_t coilId;
  uint8_t mode;
  uint8_t current;
  uint8_t driverAwake;
  uint8_t faultActive;
};

struct PduChargerState {
  uint8_t enabled;
  uint8_t chargeIndicatorActive;
  uint8_t shdnLatch;
  uint8_t chrgRaw;
};

inline uint16_t pduTestCrc16Ccitt(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
  }

  return crc;
}

inline void pduTestPutLe16(uint8_t *buffer, uint16_t value) {
  buffer[0] = (uint8_t)(value & 0xFFU);
  buffer[1] = (uint8_t)(value >> 8U);
}

inline uint16_t pduTestGetLe16(const uint8_t *buffer) {
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8U);
}

inline uint32_t pduTestGetLe32(const uint8_t *buffer) {
  return (uint32_t)buffer[0] |
         ((uint32_t)buffer[1] << 8U) |
         ((uint32_t)buffer[2] << 16U) |
         ((uint32_t)buffer[3] << 24U);
}

inline const char *pduTestStatusName(uint8_t status) {
  switch (status) {
    case PDU_V2_STATUS_OK: return "OK";
    case PDU_V2_STATUS_BAD_OPCODE: return "BAD_OPCODE";
    case PDU_V2_STATUS_BAD_LENGTH: return "BAD_LENGTH";
    case PDU_V2_STATUS_BAD_PARAM: return "BAD_PARAM";
    case PDU_V2_STATUS_HW_FAULT: return "HW_FAULT";
    case PDU_V2_STATUS_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
    case PDU_V2_STATUS_BUSY: return "BUSY";
    default: return "UNKNOWN";
  }
}

inline const char *pduTestOutputName(uint8_t outputId) {
  switch (outputId) {
    case PDU_OUTPUT_3V3_1: return "3v3_1";
    case PDU_OUTPUT_3V3_2: return "3v3_2";
    case PDU_OUTPUT_5V_1: return "5v1";
    case PDU_OUTPUT_5V_2: return "5v2";
    case PDU_OUTPUT_5V_3: return "5v3";
    case PDU_OUTPUT_12V: return "12v";
    case PDU_OUTPUT_VBATT: return "vbatt";
    case PDU_OUTPUT_BURN1: return "burn1";
    case PDU_OUTPUT_BURN2: return "burn2";
    case PDU_V2_OUTPUT_ALL: return "all";
    default: return "unknown";
  }
}

inline const char *pduTestTorqueModeName(uint8_t mode) {
  switch (mode) {
    case PDU_TORQUE_MODE_COAST: return "coast";
    case PDU_TORQUE_MODE_FORWARD: return "forward";
    case PDU_TORQUE_MODE_REVERSE: return "reverse";
    case PDU_TORQUE_MODE_BRAKE: return "brake";
    default: return "unknown";
  }
}

inline bool pduTestParseOutputId(String text, uint8_t &outputId) {
  text.toLowerCase();

  if (text == "3v3_1" || text == "sw_3v3_1") outputId = PDU_OUTPUT_3V3_1;
  else if (text == "3v3_2" || text == "sw_3v3_2") outputId = PDU_OUTPUT_3V3_2;
  else if (text == "5v1" || text == "5v_1" || text == "sw_5v_1") outputId = PDU_OUTPUT_5V_1;
  else if (text == "5v2" || text == "5v_2" || text == "sw_5v_2") outputId = PDU_OUTPUT_5V_2;
  else if (text == "5v3" || text == "5v_3" || text == "sw_5v_3") outputId = PDU_OUTPUT_5V_3;
  else if (text == "12v" || text == "sw_12v") outputId = PDU_OUTPUT_12V;
  else if (text == "vbatt") outputId = PDU_OUTPUT_VBATT;
  else if (text == "burn1") outputId = PDU_OUTPUT_BURN1;
  else if (text == "burn2") outputId = PDU_OUTPUT_BURN2;
  else if (text == "all") outputId = PDU_V2_OUTPUT_ALL;
  else return false;

  return true;
}

inline bool pduTestParseOnOff(String text, uint8_t &state) {
  text.toLowerCase();
  if (text == "on" || text == "enable" || text == "enabled" || text == "1") {
    state = 1U;
    return true;
  }
  if (text == "off" || text == "disable" || text == "disabled" || text == "0") {
    state = 0U;
    return true;
  }
  return false;
}

inline String pduTestTokenAt(String line, uint8_t index) {
  line.trim();
  uint8_t current = 0;
  int start = 0;
  int lineLength = (int)line.length();

  while (start < lineLength) {
    while (start < lineLength && line[start] == ' ') {
      start++;
    }

    int end = line.indexOf(' ', start);
    if (end < 0) {
      end = line.length();
    }

    if (current == index) {
      return line.substring(start, end);
    }

    current++;
    start = end + 1;
  }

  return "";
}

class PduTestClient {
 public:
  PduTestClient(HardwareSerial &uart, Stream &log) : uart_(uart), log_(log) {}

  void begin(uint32_t baud = PDU_TEST_UART_BAUD) {
    uart_.begin(baud);
  }

  void setDebug(bool enabled) {
    debugFrames_ = enabled;
  }

  bool request(uint8_t opcode, const uint8_t *payload, uint8_t payloadLen, PduFrame &response, uint32_t timeoutMs = 1000U) {
    uint8_t seq = nextSeq_++;
    clearRx();

    if (!sendRequest(opcode, seq, payload, payloadLen)) {
      log_.println(F("PDU request build failed"));
      return false;
    }

    return readResponse(response, opcode, seq, timeoutMs);
  }

  bool ping(PduFrame &response) {
    return request(PDU_V2_OP_PING, nullptr, 0U, response);
  }

  bool getProtocolInfo(PduProtocolInfo &info, PduFrame *raw = nullptr) {
    PduFrame response = {};
    if (!request(PDU_V2_OP_GET_PROTOCOL_INFO, nullptr, 0U, response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen < 7U) {
      return false;
    }

    info.protocolVersion = response.payload[0];
    info.capabilities = response.payload[1];
    info.maxPayload = response.payload[2];
    info.outputCount = response.payload[3];
    info.firmwareMajor = response.payload[4];
    info.firmwareMinor = response.payload[5];
    info.firmwarePatch = response.payload[6];
    return true;
  }

  bool getSummary(PduSummary &summary, PduFrame *raw = nullptr) {
    PduFrame response = {};
    if (!request(PDU_V2_OP_GET_SUMMARY_STATUS, nullptr, 0U, response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen < 9U) {
      return false;
    }

    summary.outputBitmap = pduTestGetLe16(&response.payload[0]);
    summary.resetCause = response.payload[2];
    summary.faultBitmap = response.payload[3];
    summary.uptimeSeconds = pduTestGetLe32(&response.payload[4]);
    summary.capabilities = response.payload[8];
    return true;
  }

  bool getResetInfo(uint8_t &resetCause, PduFrame *raw = nullptr) {
    PduFrame response = {};
    if (!request(PDU_V2_OP_GET_RESET_INFO, nullptr, 0U, response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen < 1U) {
      return false;
    }
    resetCause = response.payload[0];
    return true;
  }

  bool getAllOutputs(PduOutputSnapshot &snapshot, PduFrame *raw = nullptr) {
    uint8_t payload[] = { PDU_V2_OUTPUT_ALL };
    PduFrame response = {};
    if (!request(PDU_V2_OP_GET_OUTPUT_STATE, payload, sizeof(payload), response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen < 2U || response.payload[0] != PDU_V2_OUTPUT_ALL) {
      return false;
    }

    snapshot.count = response.payload[1];
    if (snapshot.count > PDU_TEST_OUTPUT_COUNT) {
      snapshot.count = PDU_TEST_OUTPUT_COUNT;
    }
    for (uint8_t i = 0; i < snapshot.count; i++) {
      snapshot.states[i] = response.payload[2U + i];
    }
    return true;
  }

  bool setOutput(uint8_t outputId, uint8_t state, PduFrame *raw = nullptr) {
    uint8_t payload[] = { outputId, state };
    PduFrame response = {};
    bool ok = request(PDU_V2_OP_SET_OUTPUT_STATE, payload, sizeof(payload), response);
    if (raw != nullptr) {
      *raw = response;
    }
    return ok && response.status == PDU_V2_STATUS_OK;
  }

  bool powerCycleOutput(uint8_t outputId, uint16_t offMs, PduFrame *raw = nullptr) {
    uint8_t payload[3];
    payload[0] = outputId;
    pduTestPutLe16(&payload[1], offMs);
    PduFrame response = {};
    bool ok = request(PDU_V2_OP_POWER_CYCLE_OUTPUT, payload, sizeof(payload), response);
    if (raw != nullptr) {
      *raw = response;
    }
    return ok && response.status == PDU_V2_STATUS_OK;
  }

  bool getTorque(uint8_t coilId, PduTorqueState &state, PduFrame *raw = nullptr) {
    uint8_t payload[] = { coilId };
    PduFrame response = {};
    if (!request(PDU_V2_OP_GET_TORQUE_COIL, payload, sizeof(payload), response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen != PDU_V2_TORQUE_COIL_RESP_LEN) {
      return false;
    }

    state.coilId = response.payload[0];
    state.mode = response.payload[1];
    state.current = response.payload[2];
    state.driverAwake = response.payload[3];
    state.faultActive = response.payload[4];
    return true;
  }

  bool setTorque(uint8_t coilId, uint8_t mode, uint8_t current, uint16_t durationMs, PduFrame *raw = nullptr) {
    uint8_t payload[5];
    payload[0] = coilId;
    payload[1] = mode;
    payload[2] = current;
    pduTestPutLe16(&payload[3], durationMs);
    PduFrame response = {};
    bool ok = request(PDU_V2_OP_SET_TORQUE_COIL, payload, sizeof(payload), response);
    if (raw != nullptr) {
      *raw = response;
    }
    return ok && response.status == PDU_V2_STATUS_OK;
  }

  bool getCharger(PduChargerState &state, PduFrame *raw = nullptr) {
    PduFrame response = {};
    if (!request(PDU_V2_OP_GET_CHARGER_STATUS, nullptr, 0U, response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen != PDU_V2_CHARGER_STATUS_RESP_LEN) {
      return false;
    }

    state.enabled = response.payload[0];
    state.chargeIndicatorActive = response.payload[1];
    state.shdnLatch = response.payload[2];
    state.chrgRaw = response.payload[3];
    return true;
  }

  bool setCharger(uint8_t enabled, PduChargerState *state = nullptr, PduFrame *raw = nullptr) {
    uint8_t payload[] = { (uint8_t)(enabled ? 1U : 0U) };
    PduFrame response = {};
    if (!request(PDU_V2_OP_SET_CHARGER_STATE, payload, sizeof(payload), response)) {
      return false;
    }
    if (raw != nullptr) {
      *raw = response;
    }
    if (response.status != PDU_V2_STATUS_OK || response.payloadLen != PDU_V2_CHARGER_STATUS_RESP_LEN) {
      return false;
    }
    if (state != nullptr) {
      state->enabled = response.payload[0];
      state->chargeIndicatorActive = response.payload[1];
      state->shdnLatch = response.payload[2];
      state->chrgRaw = response.payload[3];
    }
    return true;
  }

 private:
  HardwareSerial &uart_;
  Stream &log_;
  uint8_t nextSeq_ = 1U;
  bool debugFrames_ = false;

  void clearRx() {
    while (uart_.available() > 0) {
      (void)uart_.read();
    }
  }

  bool readByteWithTimeout(uint8_t &value, uint32_t timeoutMs) {
    uint32_t startMs = millis();
    while ((millis() - startMs) < timeoutMs) {
      if (uart_.available() > 0) {
        value = (uint8_t)uart_.read();
        return true;
      }
    }
    return false;
  }

  void printHexByte(uint8_t value) {
    if (value < 0x10U) {
      log_.print('0');
    }
    log_.print(value, HEX);
  }

  void printBytes(const char *label, const uint8_t *data, size_t len) {
    log_.print(label);
    log_.print(F(" ["));
    log_.print(len);
    log_.print(F("]: "));
    for (size_t i = 0; i < len; i++) {
      printHexByte(data[i]);
      if (i + 1U < len) {
        log_.print(' ');
      }
    }
    log_.println();
  }

  bool sendRequest(uint8_t opcode, uint8_t seq, const uint8_t *payload, uint8_t payloadLen) {
    if (payloadLen > PDU_V2_MAX_PAYLOAD_LEN) {
      return false;
    }

    uint8_t frame[PDU_V2_MAX_FRAME_LEN];
    size_t index = 0;

    frame[index++] = PDU_V2_SOF;
    frame[index++] = PDU_V2_VERSION;
    frame[index++] = PDU_V2_MSG_REQUEST;
    frame[index++] = opcode;
    frame[index++] = seq;
    frame[index++] = 0U;
    frame[index++] = payloadLen;

    for (uint8_t i = 0; i < payloadLen; i++) {
      frame[index++] = payload[i];
    }

    uint16_t crc = pduTestCrc16Ccitt(&frame[1], 6U + payloadLen);
    frame[index++] = (uint8_t)(crc & 0xFFU);
    frame[index++] = (uint8_t)(crc >> 8U);

    if (debugFrames_) {
      printBytes("TX", frame, index);
    }

    uart_.write(frame, index);
    uart_.flush();
    return true;
  }

  bool readResponse(PduFrame &frame, uint8_t expectedOpcode, uint8_t expectedSeq, uint32_t timeoutMs) {
    uint8_t byteValue = 0U;
    uint32_t startMs = millis();

    while (true) {
      if ((millis() - startMs) >= timeoutMs) {
        log_.println(F("RX timeout waiting for SOF"));
        return false;
      }
      if (uart_.available() > 0) {
        byteValue = (uint8_t)uart_.read();
        if (byteValue == PDU_V2_SOF) {
          break;
        }
      }
    }

    uint8_t header[6];
    for (uint8_t i = 0; i < sizeof(header); i++) {
      if (!readByteWithTimeout(header[i], timeoutMs)) {
        log_.println(F("RX timeout reading header"));
        return false;
      }
    }

    frame.version = header[0];
    frame.msgType = header[1];
    frame.opcode = header[2];
    frame.seq = header[3];
    frame.status = header[4];
    frame.payloadLen = header[5];

    if (frame.payloadLen > PDU_V2_MAX_PAYLOAD_LEN) {
      log_.println(F("RX bad payload length"));
      return false;
    }

    for (uint8_t i = 0; i < frame.payloadLen; i++) {
      if (!readByteWithTimeout(frame.payload[i], timeoutMs)) {
        log_.println(F("RX timeout reading payload"));
        return false;
      }
    }

    uint8_t crcBytes[2];
    if (!readByteWithTimeout(crcBytes[0], timeoutMs) ||
        !readByteWithTimeout(crcBytes[1], timeoutMs)) {
      log_.println(F("RX timeout reading CRC"));
      return false;
    }

    uint8_t crcInput[6U + PDU_V2_MAX_PAYLOAD_LEN];
    memcpy(crcInput, header, sizeof(header));
    memcpy(&crcInput[6], frame.payload, frame.payloadLen);

    uint16_t expectedCrc = pduTestGetLe16(crcBytes);
    uint16_t actualCrc = pduTestCrc16Ccitt(crcInput, 6U + frame.payloadLen);
    if (actualCrc != expectedCrc) {
      log_.println(F("RX CRC mismatch"));
      return false;
    }

    if (debugFrames_) {
      uint8_t rawFrame[PDU_V2_MAX_FRAME_LEN];
      rawFrame[0] = PDU_V2_SOF;
      memcpy(&rawFrame[1], header, sizeof(header));
      memcpy(&rawFrame[7], frame.payload, frame.payloadLen);
      rawFrame[7U + frame.payloadLen] = crcBytes[0];
      rawFrame[8U + frame.payloadLen] = crcBytes[1];
      printBytes("RX", rawFrame, 9U + frame.payloadLen);
    }

    if (frame.version != PDU_V2_VERSION ||
        frame.msgType != PDU_V2_MSG_RESPONSE ||
        frame.opcode != expectedOpcode ||
        frame.seq != expectedSeq) {
      log_.println(F("RX response mismatch"));
      return false;
    }

    return true;
  }
};

#endif /* PDU_TEST_COMMON_H */
