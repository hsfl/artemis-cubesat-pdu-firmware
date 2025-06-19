/*
 * Artemis PDU Firmware - artemisqueues Management Module
 * ------------------------------------------------------
 * This file implements FreeRTOS queues for the Artemis Power Distribution Unit (PDU).
 *
 * Note: This file is named 'artemisqueues' to avoid confusion with FreeRTOS's queue.c/h files.
 *
 * Queues provided:
 *   - xQueueSystemEvents: For system-level events (e.g., OVER_TEMP, WDT_FAULT)
 *   - xQueueTelemetry: For telemetry samples (voltages, temps, current)
 *   - (Planned) xQueueCommands: For incoming decoded commands (e.g., SetSwitch)
 *
 * Usage:
 *   - Call artemisqueues_initialize() during system initialization to create all queues.
 *   - Use the provided queue handles for inter-task communication.
 *
 * Note:
 *   - Replace uint32_t with actual event/telemetry/command structs as needed.
 *
 * (C) Artemis CubeSat Project
 */
#include "artemisqueues.h"

// Define the queue handles
QueueHandle_t xQueueSystemEvents = NULL;
QueueHandle_t xQueueTelemetry = NULL;
// QueueHandle_t xQueueCommands = NULL;

#define SYSTEM_EVENT_ITEM_SIZE sizeof(uint32_t)
#define TELEMETRY_QUEUE_ITEM_SIZE sizeof(pdu_telemetry_t)
// #define COMMAND_ITEM_SIZE sizeof(uint32_t)

#define SYSTEM_EVENT_QUEUE_LENGTH 8
#define TELEMETRY_QUEUE_LENGTH 8
// #define COMMAND_QUEUE_LENGTH 8

void artemisqueues_initialize(void)
{
    xQueueSystemEvents = xQueueCreate(SYSTEM_EVENT_QUEUE_LENGTH, SYSTEM_EVENT_ITEM_SIZE);
    xQueueTelemetry = xQueueCreate(TELEMETRY_QUEUE_LENGTH, TELEMETRY_QUEUE_ITEM_SIZE);
    // xQueueCommands = xQueueCreate(COMMAND_QUEUE_LENGTH, COMMAND_ITEM_SIZE);
} 