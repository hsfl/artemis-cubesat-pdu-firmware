# AGENTS.md

## Scope

This repository contains Artemis CubeSat PDU firmware for a Microchip ATSAME51 MCU. It is an MPLAB Harmony v3 project with FreeRTOS task startup code, Harmony-generated peripheral/configuration code, and a small handwritten application/protocol layer under `src/`.

## Code Map

- `src/main.c`: Harmony entry point. Calls `SYS_Initialize(NULL)`, optionally prints the firmware version, then loops on `SYS_Tasks()`.
- `src/config/default/tasks.c`: Creates the FreeRTOS tasks for `SYS_FS`, `DRV_SDSPI`, the watchdog, and `APP_Tasks`, then starts the scheduler.
- `src/app.c` / `src/app.h`: Main handwritten application logic. Handles startup GPIO state, UART polling, RTC/I2C init, and optional FATFS helpers. `APP_Tasks()` currently only polls `USART_READ()`.
- `src/pdu_packet.c` / `src/pdu_packet.h`: Active framed protocol parser/encoder and GPIO state control for output commands and telemetry.
- `src/pdu_protocol_v2.h`: Current local protocol definition for framed UART communication with explicit opcodes, output IDs, and status codes.
- `src/artemis-cubesat-protocols/pdu/pdu_protocol.h`: Shared protocol header, tracked as a Git submodule. This is the on-wire contract source of truth unless a task explicitly says otherwise.
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
- If a change affects the command protocol, keep `src/pdu_packet.c` behavior aligned with `src/artemis-cubesat-protocols/pdu/pdu_protocol.h`.

## Firmware Behavior Notes

- Runtime path:
  - `src/main.c` -> `SYS_Tasks()` -> `lAPP_Tasks()` -> `APP_Tasks()` -> `USART_READ()` -> `pdu_protocol_process_byte()`
- `APP_Initialize()` calls `disableGPIOs()` on boot, then initializes RTC and SERCOM4 I2C, and sets the LED.
- `APP_Tasks()` is a tight polling loop; there is no delay in the app task itself.
- `USART_READ()` feeds bytes into the framed protocol parser in `src/pdu_packet.c`.
- The legacy `read_CMD()` helper still exists in `src/app.c`, but it is no longer in the active runtime path.
- Several logical outputs are composite, not single pins:
  - `SW_12V` uses both `SW_5V_EN4` and `SW_12V_EN1`
  - Burn-wire state depends on `BURN_5V` plus `BURN1_EN` and/or `BURN2_EN`
  - H-bridge states are combinations of `FAULT*`, `IN*`, `TRQ*`, and `SLEEP*`
- Startup semantics are not purely "all off":
  - `disableGPIOs()` clears most rails, but it sets `BURN1_EN`
  - verify active-high/active-low intent from `definitions.h` and board behavior before changing any GPIO default
- The active wire protocol is the framed binary protocol defined in `src/pdu_protocol_v2.h` and described in `PDU_PROTOCOL_ICD.md`.
- The older ASCII-offset shared header under `src/artemis-cubesat-protocols/` is no longer the active runtime contract for this firmware checkout.
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

- `src/artemis-cubesat-protocols` is a Git submodule in this checkout. Treat it as a separate repo unless the task explicitly asks to update shared protocol definitions too.
- Do not remove nested Git metadata, rewrite submodule configuration, or "flatten" the workspace layout unless explicitly asked.
- The repo may contain generated MPLAB/Harmony artifacts; avoid broad formatting or cleanup changes across generated code.

## Practical Agent Guidance

- Keep changes surgical. The handwritten logic surface is small; most of the repo is generated or vendor-owned.
- When touching protocol code, verify both command execution and telemetry/readback paths.
- When touching GPIO behavior, confirm whether the state is direct-pin or composite logical state before changing anything.
- When touching packet structures, reason carefully about packed layout, ASCII offset conversion, and compatibility with the shared protocol submodule.
- If a task requires Harmony/MCC regeneration, document exactly which generated areas were intentionally changed and what likely needs to be regenerated in MPLAB X.
