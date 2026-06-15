# AGENTS.md

## Scope

This repository contains Artemis CubeSat PDU firmware for a Microchip ATSAME51 MCU. It is an MPLAB Harmony v3 project with FreeRTOS task startup code, Harmony-generated peripheral/configuration code, and a small handwritten application/protocol layer under `src/`.

## Code Map

- `src/main.c`: Harmony entry point. Calls `SYS_Initialize(NULL)`, optionally prints the firmware version, then loops on `SYS_Tasks()`.
- `src/config/default/tasks.c`: Creates the FreeRTOS tasks for `SYS_FS`, `DRV_SDSPI`, the watchdog, and `APP_Tasks`, then starts the scheduler.
- `src/app.c` / `src/app.h`: Main handwritten application logic. Applies safe startup GPIO defaults, sets the status LED, and polls UART. `APP_Tasks()` currently only polls `USART_READ()`.
- `src/pdu_packet.c` / `src/pdu_packet.h`: Active framed protocol parser/encoder and GPIO state control for output commands and telemetry.
- `src/pdu_protocol_v2.h`: Shared in-repo protocol definition for framed UART communication. Firmware and the Teensy comms tester include this same header.
- `src/config/default/`: Harmony-generated configuration, peripheral drivers, system services, and `definitions.h`.
- `ArtemisPDU.X/`: MPLAB X project metadata, generated makefiles, and MCC/Harmony configuration snapshots.
- `src/packs/`, `src/third_party/`: Vendor/device pack and third-party code.

## What To Edit

- Prefer editing the handwritten firmware files first:
  - `src/app.c`
  - `src/app.h`
  - `src/pdu_packet.c`
  - `src/pdu_packet.h`
  - occasionally `src/main.c`
- Treat these areas as generated or config-owned unless the task truly requires them:
  - `src/config/default/`
  - `ArtemisPDU.X/nbproject/`
  - `ArtemisPDU.X/ArtemisPDU_default*/`
  - Harmony/MCC manifest or YAML files
- If a change affects the command protocol, keep `src/pdu_packet.c`, `src/pdu_protocol_v2.h`, and `PDU_PROTOCOL_ICD.md` aligned.

## Firmware Behavior Notes

- Runtime path:
  - `src/main.c` -> `SYS_Tasks()` -> `lAPP_Tasks()` -> `APP_Tasks()` -> `USART_READ()` -> `pdu_protocol_process_byte()`
- `APP_Initialize()` calls `disableAllGPIOs()` on boot, then sets the LED.
- `APP_Tasks()` polls UART once per app-task iteration; `lAPP_Tasks()` adds a 1 ms FreeRTOS delay between iterations.
- `USART_READ()` feeds bytes into the framed protocol parser in `src/pdu_packet.c`.
- Legacy text-command and app-level SD-card demo helpers were removed from `src/app.c`; generated Harmony SD/FATFS support remains configured for future use.
- Several logical outputs are composite, not single pins:
  - `SW_12V` uses both `SW_5V_EN4` and `SW_12V_EN1`
  - `SW_5V_EN4` is reserved as the 5 V input to the 12 V switch regulator, per the manual: "Provide 5V power to the 12V Switch Regulator. The 12V Switch Regulator circuit converts the input 5V to output 12V for the end-user to program and utilize."
  - Burn-wire state depends on `BURN_5V` plus `BURN1_EN` and/or `BURN2_EN`
  - H-bridge states are combinations of `FAULT*`, `IN*`, `TRQ*`, and `SLEEP*`
- Startup uses the same `disableAllGPIOs()` helper as protocol-level all-off behavior.
- Verify active-high/active-low intent from `definitions.h` and board behavior before changing any GPIO default.
- The active wire protocol is the framed binary protocol defined in `src/pdu_protocol_v2.h` and described in `PDU_PROTOCOL_ICD.md`.
- `src/pdu_protocol_v2.h` is the single in-repo protocol header. Do not duplicate active opcode, status, or output constants in test sketches.
- Packet layout still matters:
  - use fixed-width fields only on the wire
  - preserve CRC coverage and frame structure
  - keep output ID ordering aligned with the ICD summary bitmap and all-output responses

## Build And Validation

- This checkout includes an MPLAB X project and generated makefiles under `ArtemisPDU.X/`.
- Do not assume the firmware is buildable from the terminal unless the required MPLAB XC32 and Harmony environment is actually installed locally.
- Only claim build success if you actually built it with the local toolchain.
- If you cannot build, validate code changes by reasoning through:
  - startup behavior in `APP_Initialize()`
  - UART receive flow in `USART_READ()`
  - task creation and watchdog behavior in `src/config/default/tasks.c`
  - command handling and telemetry in `src/pdu_packet.c`
  - any relevant pin macros or peripheral APIs from `src/config/default/definitions.h`
- No automated unit-test suite is committed in this repo.

## Git And Workspace Notes

- There is intentionally no active external protocol dependency in this checkout. Keep the PDU v2 protocol ground truth in this repo until there are multiple real production consumers and explicit versioning ownership.
- The repo may contain generated MPLAB/Harmony artifacts; avoid broad formatting or cleanup changes across generated code.

## Practical Agent Guidance

- Keep changes surgical. The handwritten logic surface is small; most of the repo is generated or vendor-owned.
- The Teensy comms tester uses a sketch-local symlink to `src/pdu_protocol_v2.h`; do not replace it with duplicated constants.
- When touching protocol code, verify both command execution and telemetry/readback paths.
- When touching GPIO behavior, confirm whether the state is direct-pin or composite logical state before changing anything.
- When touching packet structures, reason carefully about fixed-width layout, CRC coverage, and compatibility with the Teensy comms tester.
- If a task requires Harmony/MCC regeneration, document exactly which generated areas were intentionally changed and what likely needs to be regenerated in MPLAB X.
