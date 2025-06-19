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
//extern QueueHandle_t xQueueCommands;

void artemisqueues_initialize(void);

#endif // ARTEMISQUEUES_H 