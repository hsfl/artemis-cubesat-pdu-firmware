/*
  Artemis PDU v2 UART Comms Test

  Purpose:
    - Manual bench test for the refactored PDU framed UART protocol.
    - Keep the protocol code readable so it can later inform F Prime integration.

  Hardware:
    - USB Serial is the student/operator console.
    - Serial1 is the Teensy <-> PDU UART link.

  Important:
    - This replaces the old ASCII/newline pdu_comm.ino protocol.
    - Frames are binary and CRC protected.
*/

#include <Arduino.h>
#include "pdu_protocol_v2.h"

const uint32_t CONSOLE_BAUD = 9600;
const uint32_t PDU_UART_BAUD = 9600;

#define PDU_UART Serial1

const uint8_t OUTPUT_ORDER[] = {
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

const size_t OUTPUT_COUNT = sizeof(OUTPUT_ORDER) / sizeof(OUTPUT_ORDER[0]);

struct PduFrame {
  uint8_t version;
  uint8_t msgType;
  uint8_t opcode;
  uint8_t seq;
  uint8_t status;
  uint8_t payloadLen;
  uint8_t payload[PDU_V2_MAX_PAYLOAD_LEN];
};

uint8_t nextSeq = 1;
bool debugFrames = false;

uint16_t crc16Ccitt(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;

    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x8000) != 0) {
        crc = (uint16_t)((crc << 1) ^ 0x1021);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

void putLe16(uint8_t *buffer, uint16_t value) {
  buffer[0] = (uint8_t)(value & 0xFF);
  buffer[1] = (uint8_t)(value >> 8);
}

uint16_t getLe16(const uint8_t *buffer) {
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

uint32_t getLe32(const uint8_t *buffer) {
  return (uint32_t)buffer[0] |
         ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) |
         ((uint32_t)buffer[3] << 24);
}

void clearPduRx() {
  while (PDU_UART.available() > 0) {
    (void)PDU_UART.read();
  }
}

bool readByteWithTimeout(uint8_t &value, uint32_t timeoutMs) {
  uint32_t startMs = millis();

  while ((millis() - startMs) < timeoutMs) {
    if (PDU_UART.available() > 0) {
      value = (uint8_t)PDU_UART.read();
      return true;
    }
  }

  return false;
}

void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

void printPayload(const uint8_t *data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    printHexByte(data[i]);
    if (i + 1 < len) {
      Serial.print(' ');
    }
  }
}

void printBytes(const char *label, const uint8_t *data, size_t len) {
  Serial.print(label);
  Serial.print(F(" ["));
  Serial.print(len);
  Serial.print(F("]: "));

  for (size_t i = 0; i < len; i++) {
    printHexByte(data[i]);
    if (i + 1 < len) {
      Serial.print(' ');
    }
  }

  Serial.println();
}

const char *statusName(uint8_t status) {
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

const char *outputName(uint8_t outputId) {
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

const char *modeName(uint8_t mode) {
  switch (mode) {
    case PDU_TORQUE_MODE_COAST: return "coast";
    case PDU_TORQUE_MODE_FORWARD: return "forward";
    case PDU_TORQUE_MODE_REVERSE: return "reverse";
    case PDU_TORQUE_MODE_BRAKE: return "brake";
    default: return "unknown";
  }
}

bool sendRequest(uint8_t opcode, uint8_t seq, const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen > PDU_V2_MAX_PAYLOAD_LEN) {
    return false;
  }

  uint8_t frame[1 + 6 + PDU_V2_MAX_PAYLOAD_LEN + 2];
  size_t index = 0;

  frame[index++] = PDU_V2_SOF;
  frame[index++] = PDU_V2_VERSION;
  frame[index++] = PDU_V2_MSG_REQUEST;
  frame[index++] = opcode;
  frame[index++] = seq;
  frame[index++] = 0;
  frame[index++] = payloadLen;

  for (uint8_t i = 0; i < payloadLen; i++) {
    frame[index++] = payload[i];
  }

  uint16_t crc = crc16Ccitt(&frame[1], 6 + payloadLen);
  frame[index++] = (uint8_t)(crc & 0xFF);
  frame[index++] = (uint8_t)(crc >> 8);

  if (debugFrames) {
    printBytes("TX", frame, index);
  }

  PDU_UART.write(frame, index);
  PDU_UART.flush();
  return true;
}

bool readResponse(PduFrame &frame, uint8_t expectedOpcode, uint8_t expectedSeq, uint32_t timeoutMs) {
  uint8_t byteValue = 0;
  uint32_t startMs = millis();

  // Wait for SOF. Bytes before SOF are ignored.
  while (true) {
    if ((millis() - startMs) >= timeoutMs) {
      Serial.println(F("RX timeout: no SOF byte 0xA5 received."));
      return false;
    }

    if (PDU_UART.available() > 0) {
      byteValue = (uint8_t)PDU_UART.read();
      if (debugFrames && byteValue != PDU_V2_SOF) {
        Serial.print(F("RX ignored byte before SOF: 0x"));
        printHexByte(byteValue);
        Serial.println();
      }
      if (byteValue == PDU_V2_SOF) {
        break;
      }
    }
  }

  uint8_t header[6];
  for (uint8_t i = 0; i < sizeof(header); i++) {
    if (!readByteWithTimeout(header[i], timeoutMs)) {
      Serial.print(F("RX timeout: partial header, got "));
      Serial.print(i);
      Serial.println(F(" of 6 bytes after SOF."));
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
    Serial.print(F("RX bad payload length: "));
    Serial.println(frame.payloadLen);
    return false;
  }

  for (uint8_t i = 0; i < frame.payloadLen; i++) {
    if (!readByteWithTimeout(frame.payload[i], timeoutMs)) {
      Serial.print(F("RX timeout: partial payload, got "));
      Serial.print(i);
      Serial.print(F(" of "));
      Serial.print(frame.payloadLen);
      Serial.println(F(" bytes."));
      return false;
    }
  }

  uint8_t crcBytes[2];
  if (!readByteWithTimeout(crcBytes[0], timeoutMs)) {
    Serial.println(F("RX timeout: missing CRC byte 0."));
    return false;
  }
  if (!readByteWithTimeout(crcBytes[1], timeoutMs)) {
    Serial.println(F("RX timeout: missing CRC byte 1."));
    return false;
  }

  if (debugFrames) {
    uint8_t rawFrame[1 + 6 + PDU_V2_MAX_PAYLOAD_LEN + 2];
    rawFrame[0] = PDU_V2_SOF;
    memcpy(&rawFrame[1], header, sizeof(header));
    memcpy(&rawFrame[7], frame.payload, frame.payloadLen);
    rawFrame[7 + frame.payloadLen] = crcBytes[0];
    rawFrame[8 + frame.payloadLen] = crcBytes[1];
    printBytes("RX", rawFrame, 9 + frame.payloadLen);
  }

  uint8_t crcInput[6 + PDU_V2_MAX_PAYLOAD_LEN];
  memcpy(crcInput, header, sizeof(header));
  memcpy(&crcInput[6], frame.payload, frame.payloadLen);

  uint16_t expectedCrc = getLe16(crcBytes);
  uint16_t actualCrc = crc16Ccitt(crcInput, 6 + frame.payloadLen);

  if (actualCrc != expectedCrc) {
    Serial.print(F("CRC mismatch. expected=0x"));
    Serial.print(expectedCrc, HEX);
    Serial.print(F(" actual=0x"));
    Serial.println(actualCrc, HEX);
    return false;
  }

  if (frame.version != PDU_V2_VERSION || frame.msgType != PDU_V2_MSG_RESPONSE) {
    Serial.print(F("RX wrong frame type/version. version="));
    Serial.print(frame.version);
    Serial.print(F(" msg_type="));
    Serial.println(frame.msgType);
    return false;
  }

  if (frame.opcode != expectedOpcode || frame.seq != expectedSeq) {
    Serial.print(F("RX response mismatch. opcode=0x"));
    printHexByte(frame.opcode);
    Serial.print(F(" expected=0x"));
    printHexByte(expectedOpcode);
    Serial.print(F(" seq="));
    Serial.print(frame.seq);
    Serial.print(F(" expected_seq="));
    Serial.println(expectedSeq);
    return false;
  }

  return true;
}

bool requestResponse(uint8_t opcode, const uint8_t *payload, uint8_t payloadLen, PduFrame &response) {
  uint8_t seq = nextSeq++;
  clearPduRx();

  if (!sendRequest(opcode, seq, payload, payloadLen)) {
    Serial.println(F("Could not build request frame."));
    return false;
  }

  if (!readResponse(response, opcode, seq, 1000)) {
    Serial.println(F("No valid response from PDU. Check wiring, baud, PDU power, and PDU firmware."));
    return false;
  }

  Serial.print(F("status: "));
  Serial.println(statusName(response.status));

  return true;
}

String tokenAt(String line, uint8_t index) {
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

bool parseOutputId(String text, uint8_t &outputId) {
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

bool parseOnOff(String text, uint8_t &state) {
  text.toLowerCase();

  if (text == "on" || text == "1") {
    state = 1;
    return true;
  }

  if (text == "off" || text == "0") {
    state = 0;
    return true;
  }

  return false;
}

bool parseChargerState(String text, uint8_t &state) {
  text.toLowerCase();

  if (text == "on" || text == "enable" || text == "enabled" || text == "1") {
    state = 1;
    return true;
  }

  if (text == "off" || text == "disable" || text == "disabled" || text == "0") {
    state = 0;
    return true;
  }

  return false;
}

bool parseTorqueMode(String text, uint8_t &mode) {
  text.toLowerCase();

  if (text == "coast" || text == "off") mode = PDU_TORQUE_MODE_COAST;
  else if (text == "forward" || text == "fwd") mode = PDU_TORQUE_MODE_FORWARD;
  else if (text == "reverse" || text == "rev") mode = PDU_TORQUE_MODE_REVERSE;
  else if (text == "brake") mode = PDU_TORQUE_MODE_BRAKE;
  else return false;

  return true;
}

bool parseTorqueCurrent(String text, uint8_t &current) {
  text.toLowerCase();

  if (text == "100" || text == "100%") current = PDU_TORQUE_CURRENT_100;
  else if (text == "50" || text == "50%") current = PDU_TORQUE_CURRENT_50;
  else return false;

  return true;
}

void printOutputs() {
  Serial.println(F("Outputs:"));
  Serial.println(F("  3v3_1, 3v3_2"));
  Serial.println(F("  5v1, 5v2, 5v3"));
  Serial.println(F("  12v"));
  Serial.println(F("  vbatt"));
  Serial.println(F("  burn1, burn2   use: burn <burn1|burn2> <ms> arm"));
  Serial.println(F("  all            readback only"));
}

void printHelp() {
  Serial.println();
  Serial.println(F("Artemis PDU v2 Teensy Comms Test"));
  Serial.println(F("Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  debug <on|off>"));
  Serial.println(F("  sniff [ms]"));
  Serial.println(F("  loopback"));
  Serial.println(F("  pdu-help"));
  Serial.println(F("  ping"));
  Serial.println(F("  info"));
  Serial.println(F("  summary"));
  Serial.println(F("  reset-info"));
  Serial.println(F("  outputs"));
  Serial.println(F("  get <output|all>"));
  Serial.println(F("  set <output> <on|off>"));
  Serial.println(F("  cycle <output> <off_ms>"));
  Serial.println(F("  torque <coil 1-4> <coast|forward|reverse|brake> <100|50> [duration_ms]"));
  Serial.println(F("  torque? <coil 1-4>"));
  Serial.println(F("  charger?"));
  Serial.println(F("  charger <on|off>"));
  Serial.println(F("  burn <burn1|burn2> <duration_ms> arm"));
  Serial.println(F("  reset-pdu arm"));
  Serial.println();
  printOutputs();
  Serial.println();
}

void printOutputStatePayload(const PduFrame &response) {
  if (response.status != PDU_V2_STATUS_OK) {
    return;
  }

  if (response.payloadLen < 2) {
    Serial.println(F("Bad output payload length."));
    return;
  }

  if (response.payload[0] == PDU_V2_OUTPUT_ALL) {
    uint8_t count = response.payload[1];
    Serial.println(F("All output states:"));

    for (uint8_t i = 0; i < count && i < OUTPUT_COUNT; i++) {
      Serial.print(F("  "));
      Serial.print(outputName(OUTPUT_ORDER[i]));
      Serial.print(F(": "));
      Serial.println(response.payload[2 + i] ? F("on") : F("off"));
    }

    return;
  }

  Serial.print(outputName(response.payload[0]));
  Serial.print(F(": "));
  Serial.println(response.payload[1] ? F("on") : F("off"));
}

void printTorquePayload(const PduFrame &response) {
  if (response.status != PDU_V2_STATUS_OK) {
    return;
  }

  if (response.payloadLen != 5) {
    Serial.println(F("Bad torque payload length."));
    return;
  }

  Serial.print(F("coil "));
  Serial.print(response.payload[0]);
  Serial.print(F(": mode="));
  Serial.print(modeName(response.payload[1]));
  Serial.print(F(" current="));
  Serial.print(response.payload[2] == PDU_TORQUE_CURRENT_50 ? F("50%") : F("100%"));
  Serial.print(F(" driver="));
  Serial.print(response.payload[3] ? F("awake") : F("sleep"));
  Serial.print(F(" fault="));
  Serial.println(response.payload[4] ? F("yes") : F("no"));
}

void printChargerPayload(const PduFrame &response) {
  if (response.status != PDU_V2_STATUS_OK) {
    return;
  }

  if (response.payloadLen != PDU_V2_CHARGER_STATUS_RESP_LEN) {
    Serial.println(F("Bad charger payload length."));
    return;
  }

  Serial.print(F("charger: "));
  Serial.println(response.payload[0] ? F("enabled") : F("shutdown"));
  Serial.print(F("charge indicator: "));
  Serial.println(response.payload[1] ? F("active") : F("not active"));
  Serial.print(F("SHDN latch: "));
  Serial.println(response.payload[2] ? F("high") : F("low"));
  Serial.print(F("raw CHRG: "));
  Serial.println(response.payload[3] ? F("high") : F("low"));
}

void commandDebug(String line) {
  uint8_t state;

  if (!parseOnOff(tokenAt(line, 1), state)) {
    Serial.println(F("Usage: debug <on|off>"));
    return;
  }

  debugFrames = (state != 0);
  Serial.print(F("debug: "));
  Serial.println(debugFrames ? F("on") : F("off"));
}

void commandSniff(String line) {
  uint32_t durationMs = (uint32_t)tokenAt(line, 1).toInt();

  if (durationMs == 0) {
    durationMs = 3000;
  }

  Serial.print(F("Sniffing PDU UART RX for "));
  Serial.print(durationMs);
  Serial.println(F(" ms. Any bytes seen below are raw."));

  uint32_t startMs = millis();
  uint32_t count = 0;

  while ((millis() - startMs) < durationMs) {
    while (PDU_UART.available() > 0) {
      uint8_t value = (uint8_t)PDU_UART.read();
      printHexByte(value);
      Serial.print(' ');
      count++;

      if ((count % 16) == 0) {
        Serial.println();
      }
    }
  }

  if ((count % 16) != 0) {
    Serial.println();
  }

  Serial.print(F("raw bytes received: "));
  Serial.println(count);
}

void commandLoopback() {
  const uint8_t testBytes[] = { 0xA5, 0x02, 0x55, 0x00, 0xFF };
  uint8_t received[sizeof(testBytes)];
  size_t receivedCount = 0;

  clearPduRx();

  Serial.println(F("Loopback test: disconnect PDU and jumper Teensy Serial1 TX to Serial1 RX."));
  Serial.println(F("Sending test bytes on Serial1..."));

  PDU_UART.write(testBytes, sizeof(testBytes));
  PDU_UART.flush();

  uint32_t startMs = millis();
  while ((millis() - startMs) < 500 && receivedCount < sizeof(received)) {
    if (PDU_UART.available() > 0) {
      received[receivedCount++] = (uint8_t)PDU_UART.read();
    }
  }

  printBytes("loopback expected", testBytes, sizeof(testBytes));
  printBytes("loopback received", received, receivedCount);

  if (receivedCount != sizeof(testBytes) ||
      memcmp(testBytes, received, sizeof(testBytes)) != 0) {
    Serial.println(F("loopback: FAIL"));
    return;
  }

  Serial.println(F("loopback: PASS"));
}

void commandPing() {
  PduFrame response;

  if (!requestResponse(PDU_V2_OP_PING, nullptr, 0, response)) {
    return;
  }

  if (response.status == PDU_V2_STATUS_OK && response.payloadLen >= 1) {
    Serial.print(F("protocol version: "));
    Serial.println(response.payload[0]);
  }
}

void commandInfo() {
  PduFrame response;

  if (!requestResponse(PDU_V2_OP_GET_PROTOCOL_INFO, nullptr, 0, response)) {
    return;
  }

  if (response.status != PDU_V2_STATUS_OK || response.payloadLen < 7) {
    return;
  }

  Serial.print(F("protocol version: "));
  Serial.println(response.payload[0]);
  Serial.print(F("capabilities: 0x"));
  printHexByte(response.payload[1]);
  Serial.println();
  Serial.print(F("max payload: "));
  Serial.println(response.payload[2]);
  Serial.print(F("output count: "));
  Serial.println(response.payload[3]);
  Serial.print(F("firmware: "));
  Serial.print(response.payload[4]);
  Serial.print('.');
  Serial.print(response.payload[5]);
  Serial.print('.');
  Serial.println(response.payload[6]);
}

void commandPduHelp() {
  PduFrame response;

  if (!requestResponse(PDU_V2_OP_HELP, nullptr, 0, response)) {
    return;
  }

  if (response.status != PDU_V2_STATUS_OK) {
    return;
  }

  Serial.print(F("PDU help: "));
  for (uint8_t i = 0; i < response.payloadLen; i++) {
    Serial.write(response.payload[i]);
  }
  Serial.println();
}

void commandSummary() {
  PduFrame response;

  if (!requestResponse(PDU_V2_OP_GET_SUMMARY_STATUS, nullptr, 0, response)) {
    return;
  }

  if (response.status != PDU_V2_STATUS_OK || response.payloadLen < 9) {
    return;
  }

  uint16_t outputBitmap = getLe16(&response.payload[0]);
  uint8_t resetCause = response.payload[2];
  uint8_t faultBitmap = response.payload[3];
  uint32_t uptimeSeconds = getLe32(&response.payload[4]);
  uint8_t capabilities = response.payload[8];

  Serial.print(F("uptime: "));
  Serial.print(uptimeSeconds);
  Serial.println(F(" s"));
  Serial.print(F("reset cause: 0x"));
  printHexByte(resetCause);
  Serial.println();
  Serial.print(F("fault bitmap: 0x"));
  printHexByte(faultBitmap);
  Serial.println();
  Serial.print(F("capabilities: 0x"));
  printHexByte(capabilities);
  Serial.println();

  Serial.println(F("output bitmap:"));
  for (size_t i = 0; i < OUTPUT_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(outputName(OUTPUT_ORDER[i]));
    Serial.print(F(": "));
    Serial.println((outputBitmap & (1U << i)) ? F("on") : F("off"));
  }
}

void commandResetInfo() {
  PduFrame response;

  if (!requestResponse(PDU_V2_OP_GET_RESET_INFO, nullptr, 0, response)) {
    return;
  }

  if (response.status == PDU_V2_STATUS_OK && response.payloadLen >= 1) {
    Serial.print(F("reset cause: 0x"));
    printHexByte(response.payload[0]);
    Serial.println();
  }
}

void commandGetOutput(String line) {
  uint8_t outputId;
  String outputText = tokenAt(line, 1);

  if (!parseOutputId(outputText, outputId)) {
    Serial.println(F("Usage: get <output|all>"));
    return;
  }

  uint8_t payload[] = { outputId };
  PduFrame response;

  if (requestResponse(PDU_V2_OP_GET_OUTPUT_STATE, payload, sizeof(payload), response)) {
    printOutputStatePayload(response);
  }
}

void commandSetOutput(String line) {
  uint8_t outputId;
  uint8_t state;

  if (!parseOutputId(tokenAt(line, 1), outputId) || !parseOnOff(tokenAt(line, 2), state)) {
    Serial.println(F("Usage: set <output> <on|off>"));
    return;
  }

  uint8_t payload[] = { outputId, state };
  PduFrame response;

  if (requestResponse(PDU_V2_OP_SET_OUTPUT_STATE, payload, sizeof(payload), response)) {
    printOutputStatePayload(response);
  }
}

void commandPowerCycle(String line) {
  uint8_t outputId;
  uint16_t offMs = (uint16_t)tokenAt(line, 2).toInt();

  if (!parseOutputId(tokenAt(line, 1), outputId) || offMs == 0) {
    Serial.println(F("Usage: cycle <output> <off_ms>"));
    return;
  }

  uint8_t payload[3];
  payload[0] = outputId;
  putLe16(&payload[1], offMs);

  PduFrame response;
  if (requestResponse(PDU_V2_OP_POWER_CYCLE_OUTPUT, payload, sizeof(payload), response)) {
    printOutputStatePayload(response);
  }
}

void commandBurn(String line) {
  uint8_t outputId;
  uint16_t fireMs = (uint16_t)tokenAt(line, 2).toInt();
  String armText = tokenAt(line, 3);
  armText.toLowerCase();

  if (!parseOutputId(tokenAt(line, 1), outputId) ||
      (outputId != PDU_OUTPUT_BURN1 && outputId != PDU_OUTPUT_BURN2) ||
      fireMs == 0 ||
      armText != "arm") {
    Serial.println(F("Usage: burn <burn1|burn2> <duration_ms> arm"));
    return;
  }

  uint8_t payload[5];
  payload[0] = outputId;
  putLe16(&payload[1], fireMs);
  putLe16(&payload[3], PDU_V2_BURN_ARM_TOKEN);

  PduFrame response;
  if (requestResponse(PDU_V2_OP_FIRE_BURN_WIRE, payload, sizeof(payload), response)) {
    printOutputStatePayload(response);
  }
}

void commandSetTorque(String line) {
  uint8_t coil = (uint8_t)tokenAt(line, 1).toInt();
  uint8_t mode;
  uint8_t current;
  uint16_t durationMs = (uint16_t)tokenAt(line, 4).toInt();

  if (coil < 1 || coil > 4 ||
      !parseTorqueMode(tokenAt(line, 2), mode) ||
      !parseTorqueCurrent(tokenAt(line, 3), current)) {
    Serial.println(F("Usage: torque <coil 1-4> <coast|forward|reverse|brake> <100|50> [duration_ms]"));
    return;
  }

  uint8_t payload[5];
  payload[0] = coil;
  payload[1] = mode;
  payload[2] = current;
  putLe16(&payload[3], durationMs);

  PduFrame response;
  if (requestResponse(PDU_V2_OP_SET_TORQUE_COIL, payload, sizeof(payload), response)) {
    printTorquePayload(response);
  }
}

void commandGetTorque(String line) {
  uint8_t coil = (uint8_t)tokenAt(line, 1).toInt();

  if (coil < 1 || coil > 4) {
    Serial.println(F("Usage: torque? <coil 1-4>"));
    return;
  }

  uint8_t payload[] = { coil };
  PduFrame response;

  if (requestResponse(PDU_V2_OP_GET_TORQUE_COIL, payload, sizeof(payload), response)) {
    printTorquePayload(response);
  }
}

void commandGetCharger() {
  PduFrame response;

  if (requestResponse(PDU_V2_OP_GET_CHARGER_STATUS, nullptr, 0, response)) {
    printChargerPayload(response);
  }
}

void commandSetCharger(String line) {
  uint8_t state;

  if (!parseChargerState(tokenAt(line, 1), state)) {
    Serial.println(F("Usage: charger <on|off>"));
    return;
  }

  uint8_t payload[] = { state };
  PduFrame response;

  if (requestResponse(PDU_V2_OP_SET_CHARGER_STATE, payload, sizeof(payload), response)) {
    printChargerPayload(response);
  }
}

void commandSoftwareReset(String line) {
  String armText = tokenAt(line, 1);
  armText.toLowerCase();

  if (armText != "arm") {
    Serial.println(F("Usage: reset-pdu arm"));
    return;
  }

  PduFrame response;
  (void)requestResponse(PDU_V2_OP_SOFTWARE_RESET, nullptr, 0, response);
}

void handleCommand(String line) {
  line.trim();
  line.toLowerCase();

  if (line.length() == 0) {
    return;
  }

  String command = tokenAt(line, 0);

  if (command == "help" || command == "?") {
    printHelp();
  } else if (command == "debug") {
    commandDebug(line);
  } else if (command == "sniff") {
    commandSniff(line);
  } else if (command == "loopback") {
    commandLoopback();
  } else if (command == "pdu-help") {
    commandPduHelp();
  } else if (command == "ping") {
    commandPing();
  } else if (command == "info") {
    commandInfo();
  } else if (command == "summary") {
    commandSummary();
  } else if (command == "reset-info") {
    commandResetInfo();
  } else if (command == "outputs") {
    printOutputs();
  } else if (command == "get") {
    commandGetOutput(line);
  } else if (command == "set") {
    commandSetOutput(line);
  } else if (command == "cycle") {
    commandPowerCycle(line);
  } else if (command == "burn") {
    commandBurn(line);
  } else if (command == "torque") {
    commandSetTorque(line);
  } else if (command == "torque?") {
    commandGetTorque(line);
  } else if (command == "charger?") {
    commandGetCharger();
  } else if (command == "charger") {
    commandSetCharger(line);
  } else if (command == "reset-pdu") {
    commandSoftwareReset(line);
  } else {
    Serial.println(F("Unknown command. Type 'help'."));
  }
}

void setup() {
  Serial.begin(CONSOLE_BAUD);
  PDU_UART.begin(PDU_UART_BAUD);

  while (!Serial) {
    delay(10);
  }

  Serial.println(F("Artemis PDU v2 Teensy Comms Test"));
  Serial.println(F("USB console: 9600 baud"));
  Serial.print(F("PDU UART: Serial1 at "));
  Serial.print(PDU_UART_BAUD);
  Serial.println(F(" baud"));
  Serial.println(F("Type 'help' for commands."));
  Serial.println();
}

void loop() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
    Serial.print(F("$ "));
  }
}
