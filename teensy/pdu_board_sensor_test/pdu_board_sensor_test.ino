/*
  Artemis PDU Board Sensor Test

  Purpose:
    - Check the TMP36 analog temperature sensor line.
    - Check the five INA219 current/power sensors over I2C.

  Notes:
    - This sketch does not talk to the PDU MCU command protocol.
    - It is a direct Teensy-side board-health test.
    - On Teensy 4.1, the old test used Wire2. Keep that default unless your
      harness routes the PDU sensor I2C bus to another Teensy I2C port.
*/

#include <Wire.h>
#include <Adafruit_INA219.h>

// USB console baud. This only affects the Arduino Serial Monitor.
const uint32_t CONSOLE_BAUD = 9600;

// TMP36 analog input. A1 is clearer than raw "1" for new students.
const uint8_t TEMP_SENSOR_PIN = A1;
const float ADC_REFERENCE_VOLTS = 3.3f;
const uint16_t ADC_MAX_COUNT = 4095;

// Teensy I2C bus connected to the PDU INA219 sensors.
#define PDU_SENSOR_WIRE Wire2

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
  { "Solar Panel 1", INA219_ADDR_SOLAR1, Adafruit_INA219(INA219_ADDR_SOLAR1), false },
  { "Solar Panel 2", INA219_ADDR_SOLAR2, Adafruit_INA219(INA219_ADDR_SOLAR2), false },
  { "Solar Panel 3", INA219_ADDR_SOLAR3, Adafruit_INA219(INA219_ADDR_SOLAR3), false },
  { "Solar Panel 4", INA219_ADDR_SOLAR4, Adafruit_INA219(INA219_ADDR_SOLAR4), false },
  { "Battery Board", INA219_ADDR_BATTERY, Adafruit_INA219(INA219_ADDR_BATTERY), false },
};

const size_t INA219_COUNT = sizeof(ina219Channels) / sizeof(ina219Channels[0]);

float tmp36VoltageFromRaw(uint16_t raw) {
  return ((float)raw * ADC_REFERENCE_VOLTS) / (float)ADC_MAX_COUNT;
}

float tmp36CelsiusFromVoltage(float voltage) {
  // TMP36: 750 mV at 25 C, 10 mV/C scale, 500 mV offset.
  return (voltage - 0.5f) * 100.0f;
}

void printDivider() {
  Serial.println();
  Serial.println(F("--------------------------------------------------"));
}

void setupIna219Sensors() {
  Serial.println(F("Checking INA219 sensors..."));

  for (size_t i = 0; i < INA219_COUNT; i++) {
    Ina219Channel &channel = ina219Channels[i];
    channel.online = channel.sensor.begin(&PDU_SENSOR_WIRE);

    Serial.print(F("  0x"));
    Serial.print(channel.address, HEX);
    Serial.print(F(" "));
    Serial.print(channel.name);
    Serial.print(F(": "));

    if (channel.online) {
      channel.sensor.setCalibration_16V_400mA();
      Serial.println(F("online"));
    } else {
      Serial.println(F("not detected"));
    }
  }
}

void printTemperatureReading() {
  uint16_t raw = analogRead(TEMP_SENSOR_PIN);
  float voltage = tmp36VoltageFromRaw(raw);
  float tempC = tmp36CelsiusFromVoltage(voltage);
  float tempF = (tempC * 9.0f / 5.0f) + 32.0f;

  Serial.println(F("TMP36 temperature input"));
  Serial.print(F("  raw: "));
  Serial.println(raw);
  Serial.print(F("  voltage: "));
  Serial.print(voltage, 3);
  Serial.println(F(" V"));

  if (voltage < 0.05f) {
    Serial.println(F("  status: likely disconnected or reading near ground"));
    return;
  }

  Serial.print(F("  temperature: "));
  Serial.print(tempC, 1);
  Serial.print(F(" C / "));
  Serial.print(tempF, 1);
  Serial.println(F(" F"));
}

void printIna219Readings() {
  Serial.println(F("INA219 current/power sensors"));

  for (size_t i = 0; i < INA219_COUNT; i++) {
    Ina219Channel &channel = ina219Channels[i];

    Serial.print(F("  "));
    Serial.print(channel.name);
    Serial.print(F(" 0x"));
    Serial.print(channel.address, HEX);
    Serial.println();

    if (!channel.online) {
      Serial.println(F("    status: not detected at startup"));
      continue;
    }

    float busVoltageV = channel.sensor.getBusVoltage_V();
    float shuntVoltageMv = channel.sensor.getShuntVoltage_mV();
    float currentMa = channel.sensor.getCurrent_mA();
    float powerMw = channel.sensor.getPower_mW();

    Serial.print(F("    bus voltage: "));
    Serial.print(busVoltageV, 3);
    Serial.println(F(" V"));
    Serial.print(F("    shunt voltage: "));
    Serial.print(shuntVoltageMv, 3);
    Serial.println(F(" mV"));
    Serial.print(F("    current: "));
    Serial.print(currentMa, 3);
    Serial.println(F(" mA"));
    Serial.print(F("    power: "));
    Serial.print(powerMw, 3);
    Serial.println(F(" mW"));
  }
}

void setup() {
  Serial.begin(CONSOLE_BAUD);
  while (!Serial) {
    delay(10);
  }

  analogReadResolution(12);
  PDU_SENSOR_WIRE.begin();

  Serial.println(F("Artemis PDU Board Sensor Test"));
  Serial.println(F("Console: USB Serial"));
  Serial.println(F("I2C bus: Wire2"));

  setupIna219Sensors();
}

void loop() {
  printDivider();
  printTemperatureReading();
  Serial.println();
  printIna219Readings();

  delay(10000);
}
