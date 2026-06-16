/*
  Artemis PDU VIBRATION Test Logger

  Purpose:
    - Boot without operator input.
    - Put the PDU in a conservative vibration-test state.
    - Log periodic PDU status to the Teensy SD card when available.

  Default policy:
    - all exposed outputs off
    - charger shutdown
    - all torque coils coast/off
    - no burn-wire firing
*/

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <stdio.h>
#include "pdu_test_common.h"

#define PDU_UART Serial1

#ifndef BUILTIN_SDCARD
#define PDU_TEST_SD_CS 10
#else
#define PDU_TEST_SD_CS BUILTIN_SDCARD
#endif

const uint32_t SAMPLE_PERIOD_MS = 5000U;
const uint32_t SAFE_REASSERT_PERIOD_MS = 60000U;

PduTestClient pdu(PDU_UART, Serial);
File logFile;
bool sdReady = false;
uint32_t lastSampleMs = 0U;
uint32_t lastSafeReassertMs = 0U;

void mirrorPrint(const char *text) {
  Serial.print(text);
  if (sdReady && logFile) {
    logFile.print(text);
  }
}

void mirrorPrintNumber(uint32_t value) {
  Serial.print(value);
  if (sdReady && logFile) {
    logFile.print(value);
  }
}

void mirrorPrintHex(uint32_t value) {
  Serial.print(value, HEX);
  if (sdReady && logFile) {
    logFile.print(value, HEX);
  }
}

void mirrorPrintln() {
  Serial.println();
  if (sdReady && logFile) {
    logFile.println();
    logFile.flush();
  }
}

void logCsvPrefix(const char *record) {
  mirrorPrintNumber(millis());
  mirrorPrint(",");
  mirrorPrint(record);
  mirrorPrint(",");
}

void logEvent(const char *record, bool ok, uint8_t status, const char *detail) {
  logCsvPrefix(record);
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(pduTestStatusName(status));
  mirrorPrint(",");
  mirrorPrint(detail);
  mirrorPrintln();
}

bool openNextLogFile() {
  char filename[13];
  for (uint8_t i = 0; i < 100U; i++) {
    snprintf(filename, sizeof(filename), "VIBE%02u.CSV", i);
    if (!SD.exists(filename)) {
      logFile = SD.open(filename, FILE_WRITE);
      if (logFile) {
        Serial.print(F("SD log file: "));
        Serial.println(filename);
        return true;
      }
    }
  }
  return false;
}

void setupLogger() {
  sdReady = SD.begin(PDU_TEST_SD_CS) && openNextLogFile();
  if (!sdReady) {
    Serial.println(F("SD logging unavailable; mirroring to USB Serial only."));
    return;
  }

  mirrorPrint("ms,record,ok,status,field1,field2,field3,field4,field5,field6,field7,field8,field9");
  mirrorPrintln();
}

void commandSafeState(const char *reason) {
  PduFrame response;
  bool ok = pdu.ping(response);
  logEvent("safe_ping", ok, ok ? response.status : 0xFFU, reason);

  for (size_t i = 0; i < PDU_TEST_OUTPUT_COUNT; i++) {
    ok = pdu.setOutput(PDU_TEST_OUTPUT_ORDER[i], 0U, &response);
    logCsvPrefix("safe_output_off");
    mirrorPrint(ok ? "1," : "0,");
    mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
    mirrorPrint(",");
    mirrorPrint(pduTestOutputName(PDU_TEST_OUTPUT_ORDER[i]));
    mirrorPrint(",0,,,,,,,");
    mirrorPrintln();
    delay(50);
  }

  for (uint8_t coil = 1U; coil <= 4U; coil++) {
    ok = pdu.setTorque(coil, PDU_TORQUE_MODE_COAST, PDU_TORQUE_CURRENT_100, 0U, &response);
    logCsvPrefix("safe_torque_coast");
    mirrorPrint(ok ? "1," : "0,");
    mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
    mirrorPrint(",");
    mirrorPrintNumber(coil);
    mirrorPrint(",coast,,,,,,,");
    mirrorPrintln();
    delay(50);
  }

  PduChargerState charger;
  ok = pdu.setCharger(0U, &charger, &response);
  logCsvPrefix("safe_charger_shutdown");
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
  mirrorPrint(",");
  mirrorPrint(ok ? "shutdown" : "failed");
  mirrorPrint(",,,,,,,,");
  mirrorPrintln();
}

void logProtocolInfo() {
  PduProtocolInfo info;
  PduFrame response;
  bool ok = pdu.getProtocolInfo(info, &response);

  logCsvPrefix("protocol_info");
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
  mirrorPrint(",");
  if (ok) {
    mirrorPrintNumber(info.protocolVersion);
    mirrorPrint(",0x");
    mirrorPrintHex(info.capabilities);
    mirrorPrint(",");
    mirrorPrintNumber(info.maxPayload);
    mirrorPrint(",");
    mirrorPrintNumber(info.outputCount);
    mirrorPrint(",");
    mirrorPrintNumber(info.firmwareMajor);
    mirrorPrint(".");
    mirrorPrintNumber(info.firmwareMinor);
    mirrorPrint(".");
    mirrorPrintNumber(info.firmwarePatch);
    mirrorPrint(",,,,");
  } else {
    mirrorPrint("no_info,,,,,,,,");
  }
  mirrorPrintln();
}

void logSummary() {
  PduSummary summary;
  PduFrame response;
  bool ok = pdu.getSummary(summary, &response);

  logCsvPrefix("summary");
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
  mirrorPrint(",");
  if (ok) {
    mirrorPrintNumber(summary.uptimeSeconds);
    mirrorPrint(",0x");
    mirrorPrintHex(summary.resetCause);
    mirrorPrint(",0x");
    mirrorPrintHex(summary.faultBitmap);
    mirrorPrint(",0x");
    mirrorPrintHex(summary.outputBitmap);
    mirrorPrint(",0x");
    mirrorPrintHex(summary.capabilities);
    mirrorPrint(",,,,");
  } else {
    mirrorPrint("no_summary,,,,,,,,");
  }
  mirrorPrintln();
}

void logOutputs() {
  PduOutputSnapshot snapshot;
  PduFrame response;
  bool ok = pdu.getAllOutputs(snapshot, &response);

  logCsvPrefix("outputs");
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
  mirrorPrint(",");
  if (ok) {
    for (uint8_t i = 0; i < PDU_TEST_OUTPUT_COUNT; i++) {
      uint8_t state = (i < snapshot.count) ? snapshot.states[i] : 0U;
      mirrorPrintNumber(state);
      if (i + 1U < PDU_TEST_OUTPUT_COUNT) {
        mirrorPrint(",");
      }
    }
  } else {
    mirrorPrint("no_outputs,,,,,,,,");
  }
  mirrorPrintln();
}

void logCharger() {
  PduChargerState charger;
  PduFrame response;
  bool ok = pdu.getCharger(charger, &response);

  logCsvPrefix("charger");
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
  mirrorPrint(",");
  if (ok) {
    mirrorPrintNumber(charger.enabled);
    mirrorPrint(",");
    mirrorPrintNumber(charger.chargeIndicatorActive);
    mirrorPrint(",");
    mirrorPrintNumber(charger.shdnLatch);
    mirrorPrint(",");
    mirrorPrintNumber(charger.chrgRaw);
    mirrorPrint(",,,,,");
  } else {
    mirrorPrint("no_charger,,,,,,,,");
  }
  mirrorPrintln();
}

void logTorque() {
  for (uint8_t coil = 1U; coil <= 4U; coil++) {
    PduTorqueState torque;
    PduFrame response;
    bool ok = pdu.getTorque(coil, torque, &response);

    logCsvPrefix("torque");
    mirrorPrint(ok ? "1," : "0,");
    mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
    mirrorPrint(",");
    mirrorPrintNumber(coil);
    mirrorPrint(",");
    if (ok) {
      mirrorPrint(pduTestTorqueModeName(torque.mode));
      mirrorPrint(",");
      mirrorPrintNumber(torque.current);
      mirrorPrint(",");
      mirrorPrintNumber(torque.driverAwake);
      mirrorPrint(",");
      mirrorPrintNumber(torque.faultActive);
      mirrorPrint(",,,,");
    } else {
      mirrorPrint("no_torque,,,,,,,");
    }
    mirrorPrintln();
  }
}

void logSample() {
  logSummary();
  logOutputs();
  logCharger();
  logTorque();
}

void setup() {
  Serial.begin(PDU_TEST_CONSOLE_BAUD);
  pdu.begin(PDU_TEST_UART_BAUD);
  delay(1000);

  Serial.println(F("Artemis PDU VIBRATION Test Logger"));
  Serial.println(F("Autonomous safe-state and SD logging profile."));
  setupLogger();
  logProtocolInfo();
  commandSafeState("boot");
  logSample();

  lastSampleMs = millis();
  lastSafeReassertMs = millis();
}

void loop() {
  uint32_t nowMs = millis();

  if ((nowMs - lastSafeReassertMs) >= SAFE_REASSERT_PERIOD_MS) {
    commandSafeState("periodic");
    lastSafeReassertMs = nowMs;
  }

  if ((nowMs - lastSampleMs) >= SAMPLE_PERIOD_MS) {
    logSample();
    lastSampleMs = nowMs;
  }
}
