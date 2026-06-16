/*
  Artemis PDU THERMAL-VAC Test Logger

  Purpose:
    - Boot without operator input.
    - Apply a deliberately configured thermal-test rail policy.
    - Log PDU status plus Teensy-side TMP36 / INA219 telemetry to SD.

  Before a real thermal-vac run:
    - Review the THERMAL_OUTPUT_PLAN constants below.
    - Keep burn-wire outputs false unless a separate approved procedure exists.
    - Confirm the Teensy has a working SD card and the sensor harness is wired.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_INA219.h>
#include <stdio.h>
#include "pdu_test_common.h"

#define PDU_UART Serial1
#define PDU_SENSOR_WIRE Wire2

#ifndef BUILTIN_SDCARD
#define PDU_TEST_SD_CS 10
#else
#define PDU_TEST_SD_CS BUILTIN_SDCARD
#endif

const uint32_t SAMPLE_PERIOD_MS = 10000U;
const uint32_t POLICY_REASSERT_PERIOD_MS = 60000U;

const uint8_t TEMP_SENSOR_PIN = A1;
const float ADC_REFERENCE_VOLTS = 3.3f;
const uint16_t ADC_MAX_COUNT = 4095U;

struct OutputPlan {
  const char *name;
  uint8_t outputId;
  bool enabled;
};

/*
  Edit this plan before thermal-vac.
  Defaults are conservative: everything off, no charger.
*/
OutputPlan THERMAL_OUTPUT_PLAN[] = {
  { "3v3_1", PDU_OUTPUT_3V3_1, false },
  { "3v3_2", PDU_OUTPUT_3V3_2, false },
  { "5v1", PDU_OUTPUT_5V_1, false },
  { "5v2", PDU_OUTPUT_5V_2, false },
  { "5v3", PDU_OUTPUT_5V_3, false },
  { "12v", PDU_OUTPUT_12V, false },
  { "vbatt", PDU_OUTPUT_VBATT, false },
  { "burn1", PDU_OUTPUT_BURN1, false },
  { "burn2", PDU_OUTPUT_BURN2, false },
};

const size_t THERMAL_OUTPUT_PLAN_COUNT = sizeof(THERMAL_OUTPUT_PLAN) / sizeof(THERMAL_OUTPUT_PLAN[0]);
const bool THERMAL_ENABLE_CHARGER = false;

const uint8_t INA219_ADDR_SOLAR1 = 0x40;
const uint8_t INA219_ADDR_SOLAR2 = 0x41;
const uint8_t INA219_ADDR_SOLAR3 = 0x42;
const uint8_t INA219_ADDR_SOLAR4 = 0x43;
const uint8_t INA219_ADDR_BATTERY = 0x44;

struct Ina219Channel {
  const char *name;
  uint8_t address;
  Adafruit_INA219 sensor;
  bool online;
};

Ina219Channel ina219Channels[] = {
  { "solar1", INA219_ADDR_SOLAR1, Adafruit_INA219(INA219_ADDR_SOLAR1), false },
  { "solar2", INA219_ADDR_SOLAR2, Adafruit_INA219(INA219_ADDR_SOLAR2), false },
  { "solar3", INA219_ADDR_SOLAR3, Adafruit_INA219(INA219_ADDR_SOLAR3), false },
  { "solar4", INA219_ADDR_SOLAR4, Adafruit_INA219(INA219_ADDR_SOLAR4), false },
  { "battery", INA219_ADDR_BATTERY, Adafruit_INA219(INA219_ADDR_BATTERY), false },
};

const size_t INA219_COUNT = sizeof(ina219Channels) / sizeof(ina219Channels[0]);

PduTestClient pdu(PDU_UART, Serial);
File logFile;
bool sdReady = false;
uint32_t lastSampleMs = 0U;
uint32_t lastPolicyReassertMs = 0U;

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

void mirrorPrintFloat(float value, uint8_t decimals) {
  Serial.print(value, decimals);
  if (sdReady && logFile) {
    logFile.print(value, decimals);
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

bool openNextLogFile() {
  char filename[13];
  for (uint8_t i = 0; i < 100U; i++) {
    snprintf(filename, sizeof(filename), "TVAC%02u.CSV", i);
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

  mirrorPrint("ms,record,ok,status,field1,field2,field3,field4,field5,field6,field7,field8,field9,field10");
  mirrorPrintln();
}

float tmp36VoltageFromRaw(uint16_t raw) {
  return ((float)raw * ADC_REFERENCE_VOLTS) / (float)ADC_MAX_COUNT;
}

float tmp36CelsiusFromVoltage(float voltage) {
  return (voltage - 0.5f) * 100.0f;
}

void setupSensors() {
  analogReadResolution(12);
  PDU_SENSOR_WIRE.begin();

  for (size_t i = 0; i < INA219_COUNT; i++) {
    Ina219Channel &channel = ina219Channels[i];
    channel.online = channel.sensor.begin(&PDU_SENSOR_WIRE);
    if (channel.online) {
      channel.sensor.setCalibration_16V_400mA();
    }

    logCsvPrefix("ina219_init");
    mirrorPrint(channel.online ? "1,OK," : "0,OFFLINE,");
    mirrorPrint(channel.name);
    mirrorPrint(",0x");
    mirrorPrintHex(channel.address);
    mirrorPrint(",,,,,,,,");
    mirrorPrintln();
  }
}

void applyThermalPolicy(const char *reason) {
  for (size_t i = 0; i < THERMAL_OUTPUT_PLAN_COUNT; i++) {
    OutputPlan &plan = THERMAL_OUTPUT_PLAN[i];
    PduFrame response;
    bool ok = pdu.setOutput(plan.outputId, plan.enabled ? 1U : 0U, &response);

    logCsvPrefix("policy_output");
    mirrorPrint(ok ? "1," : "0,");
    mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
    mirrorPrint(",");
    mirrorPrint(plan.name);
    mirrorPrint(",");
    mirrorPrintNumber(plan.enabled ? 1U : 0U);
    mirrorPrint(",");
    mirrorPrint(reason);
    mirrorPrint(",,,,,,,");
    mirrorPrintln();
    delay(50);
  }

  for (uint8_t coil = 1U; coil <= 4U; coil++) {
    PduFrame response;
    bool ok = pdu.setTorque(coil, PDU_TORQUE_MODE_COAST, PDU_TORQUE_CURRENT_100, 0U, &response);
    logCsvPrefix("policy_torque");
    mirrorPrint(ok ? "1," : "0,");
    mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
    mirrorPrint(",");
    mirrorPrintNumber(coil);
    mirrorPrint(",coast,");
    mirrorPrint(reason);
    mirrorPrint(",,,,,,,");
    mirrorPrintln();
    delay(50);
  }

  PduChargerState charger;
  PduFrame response;
  bool ok = pdu.setCharger(THERMAL_ENABLE_CHARGER ? 1U : 0U, &charger, &response);
  logCsvPrefix("policy_charger");
  mirrorPrint(ok ? "1," : "0,");
  mirrorPrint(ok ? pduTestStatusName(response.status) : "NO_RESPONSE");
  mirrorPrint(",");
  mirrorPrint(THERMAL_ENABLE_CHARGER ? "enable" : "shutdown");
  mirrorPrint(",");
  mirrorPrint(reason);
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
    mirrorPrint(",,,,,");
  } else {
    mirrorPrint("no_info,,,,,,,,,");
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
    mirrorPrint(",,,,,");
  } else {
    mirrorPrint("no_summary,,,,,,,,,");
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
    mirrorPrint(",");
  } else {
    mirrorPrint("no_outputs,,,,,,,,,");
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
    mirrorPrint(",,,,,,");
  } else {
    mirrorPrint("no_charger,,,,,,,,,");
  }
  mirrorPrintln();
}

void logTemperature() {
  uint16_t raw = analogRead(TEMP_SENSOR_PIN);
  float voltage = tmp36VoltageFromRaw(raw);
  float tempC = tmp36CelsiusFromVoltage(voltage);
  float tempF = (tempC * 9.0f / 5.0f) + 32.0f;

  logCsvPrefix("tmp36");
  mirrorPrint("1,OK,");
  mirrorPrintNumber(raw);
  mirrorPrint(",");
  mirrorPrintFloat(voltage, 4);
  mirrorPrint(",");
  mirrorPrintFloat(tempC, 2);
  mirrorPrint(",");
  mirrorPrintFloat(tempF, 2);
  mirrorPrint(",,,,,,");
  mirrorPrintln();
}

void logIna219() {
  for (size_t i = 0; i < INA219_COUNT; i++) {
    Ina219Channel &channel = ina219Channels[i];

    logCsvPrefix("ina219");
    if (!channel.online) {
      mirrorPrint("0,OFFLINE,");
      mirrorPrint(channel.name);
      mirrorPrint(",0x");
      mirrorPrintHex(channel.address);
      mirrorPrint(",,,,,,,,");
      mirrorPrintln();
      continue;
    }

    float busVoltageV = channel.sensor.getBusVoltage_V();
    float shuntVoltageMv = channel.sensor.getShuntVoltage_mV();
    float currentMa = channel.sensor.getCurrent_mA();
    float powerMw = channel.sensor.getPower_mW();

    mirrorPrint("1,OK,");
    mirrorPrint(channel.name);
    mirrorPrint(",0x");
    mirrorPrintHex(channel.address);
    mirrorPrint(",");
    mirrorPrintFloat(busVoltageV, 4);
    mirrorPrint(",");
    mirrorPrintFloat(shuntVoltageMv, 4);
    mirrorPrint(",");
    mirrorPrintFloat(currentMa, 4);
    mirrorPrint(",");
    mirrorPrintFloat(powerMw, 4);
    mirrorPrint(",,,");
    mirrorPrintln();
  }
}

void logSample() {
  logSummary();
  logOutputs();
  logCharger();
  logTemperature();
  logIna219();
}

void setup() {
  Serial.begin(PDU_TEST_CONSOLE_BAUD);
  pdu.begin(PDU_TEST_UART_BAUD);
  delay(1000);

  Serial.println(F("Artemis PDU THERMAL-VAC Test Logger"));
  Serial.println(F("Autonomous policy apply and SD logging profile."));
  setupLogger();
  setupSensors();
  logProtocolInfo();
  applyThermalPolicy("boot");
  logSample();

  lastSampleMs = millis();
  lastPolicyReassertMs = millis();
}

void loop() {
  uint32_t nowMs = millis();

  if ((nowMs - lastPolicyReassertMs) >= POLICY_REASSERT_PERIOD_MS) {
    applyThermalPolicy("periodic");
    lastPolicyReassertMs = nowMs;
  }

  if ((nowMs - lastSampleMs) >= SAMPLE_PERIOD_MS) {
    logSample();
    lastSampleMs = nowMs;
  }
}
