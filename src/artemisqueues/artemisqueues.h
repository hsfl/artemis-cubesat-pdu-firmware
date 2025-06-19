/*
 * Artemis PDU Firmware - artemisqueues Management Module (Header)
 * --------------------------------------------------------------
 * This header defines the FreeRTOS queue handles and initialization function
 * for the Artemis Power Distribution Unit (PDU).
 *
 * Note: This file is named 'artemisqueues' to avoid confusion with FreeRTOS's queue.c/h files.
 *
 * Queues provided:
 *   - xQueueSystemEvents: For system-level events (e.g., OVER_TEMP, WDT_FAULT)
 *   - xQueueTelemetry: For telemetry samples (voltages, temps, current)
 *   - (Planned) xQueueCommands: For incoming decoded commands (e.g., SetSwitch)
 *
 * Usage:
 *   - Include this header in modules that need access to Artemis PDU queues.
 *   - Call artemisqueues_initialize() during system startup.
 *
 * (C) Artemis CubeSat Project
 */
#ifndef ARTEMISQUEUES_H
#define ARTEMISQUEUES_H

#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t xQueueSystemEvents;
extern QueueHandle_t xQueueTelemetry;
// extern QueueHandle_t xQueueCommands;

// Telemetry struct for the telemetry queue
// Battery telemetry
// Solar panel telemetry (4 inputs)
// Temperatures from TMP36 sensors
// Timestamp for when the sample was taken
typedef struct
{
    // === Power Sensors (INA219 over I2C: shared SDA/SCL via PC104 H1-7/8) ===

    float vbatt_voltage; // INA219 @ 0x44 — monitors battery output voltage
    float vbatt_current; // INA219 @ 0x44 — monitors battery output current

    float solar_voltage[4]; // INA219 @ 0x40–0x43 — solar panel inputs:
                            // [0] = Solar 1 (J1), [1] = Solar 2 (J2),
                            // [2] = Solar 3 (J3), [3] = Solar 4 (J4)

    float solar_current[4]; // INA219 @ 0x40–0x43 — current from solar inputs

    // === Temperature Sensors (TMP36 analog signals via Molex to Teensy ADC) ===

    float temp_pdu;  // AIN1 — PDU temp sensor, Molex J19
    float temp_batt; // AIN2 — Battery board temp sensor, Molex J23

    float temp_solar[4]; // Solar panel face temperatures:
                         // [0] = AIN3 (J22, Solar 1)
                         // [1] = AIN4 (J19, Solar 2)
                         // [2] = AIN5 (J24, Solar 3)
                         // [3] = AIN6 (J25, Solar 4)

    // === Timing ===
    uint32_t timestamp_ms; // Time since boot (ms)
} pdu_telemetry_t;

void artemisqueues_initialize(void);

#endif // ARTEMISQUEUES_H