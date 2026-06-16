/*
  Artemis PDU ALL Bench Test

  Purpose:
    - Operator-driven checkout of the PDU framed UART protocol.
    - Exercise safe rail commands while a human measures voltages/current.
    - Avoid autonomous burn-wire or broad all-on behavior.

  Hardware:
    - USB Serial Monitor is the operator console.
    - Serial1 is the Teensy <-> PDU UART link.
*/

#include <Arduino.h>
#include "pdu_test_common.h"

#define PDU_UART Serial1

PduTestClient pdu(PDU_UART, Serial);
bool torquePulseArmed = false;

void printHelp() {
  Serial.println();
  Serial.println(F("Artemis PDU ALL Bench Test"));
  Serial.println(F("Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  run                 guided safe checkout"));
  Serial.println(F("  ping"));
  Serial.println(F("  info"));
  Serial.println(F("  summary"));
  Serial.println(F("  reset-info"));
  Serial.println(F("  get all"));
  Serial.println(F("  set <output> <on|off>"));
  Serial.println(F("  cycle <output> <off_ms>"));
  Serial.println(F("  all-off"));
  Serial.println(F("  charger?"));
  Serial.println(F("  charger <on|off>"));
  Serial.println(F("  torque? <coil 1-4>"));
  Serial.println(F("  arm-torque <on|off>"));
  Serial.println(F("  torque-pulse <coil 1-4> <forward|reverse> <100|50> <ms>"));
  Serial.println();
  Serial.println(F("Outputs: 3v3_1 3v3_2 5v1 5v2 5v3 12v vbatt burn1 burn2"));
  Serial.println(F("Burn outputs are allowed to be commanded off, but this sketch does not fire burn wires."));
  Serial.println();
}

void printFrameStatus(const char *label, bool transportOk, const PduFrame &frame) {
  Serial.print(label);
  Serial.print(F(": "));
  if (!transportOk) {
    Serial.println(F("NO RESPONSE"));
    return;
  }
  Serial.println(pduTestStatusName(frame.status));
}

void commandPing() {
  PduFrame response;
  bool ok = pdu.ping(response);
  printFrameStatus("ping", ok, response);
  if (ok && response.status == PDU_V2_STATUS_OK && response.payloadLen >= 1U) {
    Serial.print(F("protocol version: "));
    Serial.println(response.payload[0]);
  }
}

void commandInfo() {
  PduProtocolInfo info;
  PduFrame response;
  bool ok = pdu.getProtocolInfo(info, &response);
  printFrameStatus("info", ok, response);
  if (!ok) {
    return;
  }

  Serial.print(F("protocol version: "));
  Serial.println(info.protocolVersion);
  Serial.print(F("capabilities: 0x"));
  Serial.println(info.capabilities, HEX);
  Serial.print(F("max payload: "));
  Serial.println(info.maxPayload);
  Serial.print(F("output count: "));
  Serial.println(info.outputCount);
  Serial.print(F("firmware: "));
  Serial.print(info.firmwareMajor);
  Serial.print('.');
  Serial.print(info.firmwareMinor);
  Serial.print('.');
  Serial.println(info.firmwarePatch);
}

void commandSummary() {
  PduSummary summary;
  PduFrame response;
  bool ok = pdu.getSummary(summary, &response);
  printFrameStatus("summary", ok, response);
  if (!ok) {
    return;
  }

  Serial.print(F("uptime_s: "));
  Serial.println(summary.uptimeSeconds);
  Serial.print(F("reset_cause: 0x"));
  Serial.println(summary.resetCause, HEX);
  Serial.print(F("fault_bitmap: 0x"));
  Serial.println(summary.faultBitmap, HEX);
  Serial.print(F("capabilities: 0x"));
  Serial.println(summary.capabilities, HEX);
  Serial.print(F("output_bitmap: 0x"));
  Serial.println(summary.outputBitmap, HEX);
}

void commandResetInfo() {
  uint8_t resetCause = 0U;
  PduFrame response;
  bool ok = pdu.getResetInfo(resetCause, &response);
  printFrameStatus("reset-info", ok, response);
  if (ok) {
    Serial.print(F("reset_cause: 0x"));
    Serial.println(resetCause, HEX);
  }
}

void commandGetAllOutputs() {
  PduOutputSnapshot snapshot;
  PduFrame response;
  bool ok = pdu.getAllOutputs(snapshot, &response);
  printFrameStatus("get all", ok, response);
  if (!ok) {
    return;
  }

  for (uint8_t i = 0; i < snapshot.count && i < PDU_TEST_OUTPUT_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(pduTestOutputName(PDU_TEST_OUTPUT_ORDER[i]));
    Serial.print(F(": "));
    Serial.println(snapshot.states[i] ? F("on") : F("off"));
  }
}

void commandSetOutput(String line) {
  uint8_t outputId;
  uint8_t state;
  if (!pduTestParseOutputId(pduTestTokenAt(line, 1), outputId) ||
      !pduTestParseOnOff(pduTestTokenAt(line, 2), state)) {
    Serial.println(F("Usage: set <output> <on|off>"));
    return;
  }

  if ((outputId == PDU_OUTPUT_BURN1 || outputId == PDU_OUTPUT_BURN2) && state != 0U) {
    Serial.println(F("Refusing burn output ON in ALL bench sketch. Use dedicated burn command only in comms test."));
    return;
  }

  PduFrame response;
  bool ok = pdu.setOutput(outputId, state, &response);
  printFrameStatus("set", ok, response);
  Serial.println(F("Measure the rail/switch now, then command it off if this was a voltage check."));
}

void commandCycleOutput(String line) {
  uint8_t outputId;
  uint16_t offMs = (uint16_t)pduTestTokenAt(line, 2).toInt();
  if (!pduTestParseOutputId(pduTestTokenAt(line, 1), outputId) || offMs == 0U) {
    Serial.println(F("Usage: cycle <output> <off_ms>"));
    return;
  }
  if (outputId == PDU_OUTPUT_BURN1 || outputId == PDU_OUTPUT_BURN2) {
    Serial.println(F("Refusing burn output cycle in ALL bench sketch."));
    return;
  }

  PduFrame response;
  bool ok = pdu.powerCycleOutput(outputId, offMs, &response);
  printFrameStatus("cycle", ok, response);
}

void commandAllOff() {
  Serial.println(F("Commanding all exposed outputs off one by one..."));
  for (size_t i = 0; i < PDU_TEST_OUTPUT_COUNT; i++) {
    PduFrame response;
    bool ok = pdu.setOutput(PDU_TEST_OUTPUT_ORDER[i], 0U, &response);
    Serial.print(F("  "));
    Serial.print(pduTestOutputName(PDU_TEST_OUTPUT_ORDER[i]));
    Serial.print(F(": "));
    Serial.println(ok ? F("off") : F("failed"));
    delay(50);
  }
  for (uint8_t coil = 1U; coil <= 4U; coil++) {
    PduFrame response;
    bool ok = pdu.setTorque(coil, PDU_TORQUE_MODE_COAST, PDU_TORQUE_CURRENT_100, 0U, &response);
    Serial.print(F("  coil "));
    Serial.print(coil);
    Serial.print(F(": "));
    Serial.println(ok ? F("coast") : F("failed"));
    delay(50);
  }
  PduChargerState charger;
  Serial.print(F("  charger: "));
  Serial.println(pdu.setCharger(0U, &charger) ? F("shutdown") : F("failed"));
}

void commandCharger(String line) {
  String command = pduTestTokenAt(line, 0);
  if (command == "charger?") {
    PduChargerState charger;
    PduFrame response;
    bool ok = pdu.getCharger(charger, &response);
    printFrameStatus("charger", ok, response);
    if (ok) {
      Serial.print(F("enabled: "));
      Serial.println(charger.enabled);
      Serial.print(F("charge_indicator_active: "));
      Serial.println(charger.chargeIndicatorActive);
      Serial.print(F("shdn_latch: "));
      Serial.println(charger.shdnLatch);
      Serial.print(F("chrg_raw: "));
      Serial.println(charger.chrgRaw);
    }
    return;
  }

  uint8_t state;
  if (!pduTestParseOnOff(pduTestTokenAt(line, 1), state)) {
    Serial.println(F("Usage: charger <on|off>"));
    return;
  }
  PduChargerState charger;
  Serial.println(pdu.setCharger(state, &charger) ? F("charger command OK") : F("charger command failed"));
}

void commandTorqueQuery(String line) {
  uint8_t coil = (uint8_t)pduTestTokenAt(line, 1).toInt();
  if (coil < 1U || coil > 4U) {
    Serial.println(F("Usage: torque? <coil 1-4>"));
    return;
  }

  PduTorqueState state;
  PduFrame response;
  bool ok = pdu.getTorque(coil, state, &response);
  printFrameStatus("torque?", ok, response);
  if (ok) {
    Serial.print(F("coil "));
    Serial.print(coil);
    Serial.print(F(" mode="));
    Serial.print(pduTestTorqueModeName(state.mode));
    Serial.print(F(" current="));
    Serial.print(state.current == PDU_TORQUE_CURRENT_50 ? F("50") : F("100"));
    Serial.print(F(" awake="));
    Serial.print(state.driverAwake);
    Serial.print(F(" fault="));
    Serial.println(state.faultActive);
  }
}

bool parseTorqueModeText(String text, uint8_t &mode) {
  text.toLowerCase();
  if (text == "forward" || text == "fwd") {
    mode = PDU_TORQUE_MODE_FORWARD;
    return true;
  }
  if (text == "reverse" || text == "rev") {
    mode = PDU_TORQUE_MODE_REVERSE;
    return true;
  }
  return false;
}

bool parseTorqueCurrentText(String text, uint8_t &current) {
  text.toLowerCase();
  if (text == "100" || text == "100%") {
    current = PDU_TORQUE_CURRENT_100;
    return true;
  }
  if (text == "50" || text == "50%") {
    current = PDU_TORQUE_CURRENT_50;
    return true;
  }
  return false;
}

void commandTorquePulse(String line) {
  uint8_t coil = (uint8_t)pduTestTokenAt(line, 1).toInt();
  uint8_t mode;
  uint8_t current;
  uint16_t durationMs = (uint16_t)pduTestTokenAt(line, 4).toInt();

  if (!torquePulseArmed) {
    Serial.println(F("Torque pulse is locked. Run: arm-torque on"));
    return;
  }

  if (coil < 1U || coil > 4U ||
      !parseTorqueModeText(pduTestTokenAt(line, 2), mode) ||
      !parseTorqueCurrentText(pduTestTokenAt(line, 3), current) ||
      durationMs == 0U || durationMs > 1000U) {
    Serial.println(F("Usage: torque-pulse <coil 1-4> <forward|reverse> <100|50> <ms<=1000>"));
    return;
  }

  PduFrame response;
  bool ok = pdu.setTorque(coil, mode, current, durationMs, &response);
  printFrameStatus("torque-pulse", ok, response);
  torquePulseArmed = false;
  Serial.println(F("Torque pulse lock reset."));
}

void commandArmTorque(String line) {
  uint8_t state;
  if (!pduTestParseOnOff(pduTestTokenAt(line, 1), state)) {
    Serial.println(F("Usage: arm-torque <on|off>"));
    return;
  }
  torquePulseArmed = (state != 0U);
  Serial.print(F("torque pulse armed: "));
  Serial.println(torquePulseArmed ? F("yes") : F("no"));
}

void runGuidedCheckout() {
  Serial.println(F("Starting guided ALL checkout."));
  Serial.println(F("This does not fire burn wires and does not auto-enable every rail."));
  commandPing();
  commandInfo();
  commandSummary();
  commandResetInfo();
  commandGetAllOutputs();
  commandCharger("charger?");
  for (uint8_t coil = 1U; coil <= 4U; coil++) {
    String line = "torque? ";
    line += coil;
    commandTorqueQuery(line);
  }
  Serial.println(F("Guided link/status checkout complete."));
  Serial.println(F("Use set/cycle commands one rail at a time while measuring voltages."));
}

void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  String lowered = line;
  lowered.toLowerCase();
  String command = pduTestTokenAt(lowered, 0);

  if (command == "help" || command == "?") printHelp();
  else if (command == "run") runGuidedCheckout();
  else if (command == "ping") commandPing();
  else if (command == "info") commandInfo();
  else if (command == "summary") commandSummary();
  else if (command == "reset-info") commandResetInfo();
  else if (command == "get" && pduTestTokenAt(lowered, 1) == "all") commandGetAllOutputs();
  else if (command == "set") commandSetOutput(lowered);
  else if (command == "cycle") commandCycleOutput(lowered);
  else if (command == "all-off") commandAllOff();
  else if (command == "charger?" || command == "charger") commandCharger(lowered);
  else if (command == "torque?") commandTorqueQuery(lowered);
  else if (command == "arm-torque") commandArmTorque(lowered);
  else if (command == "torque-pulse") commandTorquePulse(lowered);
  else Serial.println(F("Unknown command. Type help."));
}

void setup() {
  Serial.begin(PDU_TEST_CONSOLE_BAUD);
  while (!Serial) {
    delay(10);
  }

  pdu.begin(PDU_TEST_UART_BAUD);
  Serial.println(F("Artemis PDU ALL Bench Test"));
  Serial.println(F("Type help, then run."));
  Serial.print(F("$ "));
}

void loop() {
  if (Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
    Serial.print(F("$ "));
  }
}
