# Artemis CubeSat PDU Firmware

Firmware for the Artemis CubeSat Power Distribution Unit (PDU) microcontroller.
The target MCU is a Microchip ATSAME51 running an MPLAB Harmony v3 / FreeRTOS
project.

## Purpose

This firmware owns the low-level PDU board behavior:

- safe GPIO defaults at boot
- switched rail control and readback
- burn-wire pulse control with an arm token and bounded duration
- torque-coil H-bridge command/readback support
- compact status telemetry for bring-up and demo operations
- watchdog-driven board reset behavior
- a framed UART command protocol for the Teensy/OBC side

The firmware exposes logical PDU outputs, not raw pin choreography. Board-specific
details such as composite rails and shared enable pins stay inside this firmware.

## System Role

The intended flight-software boundary is:

```text
F Prime EPS component
  -> Teensy / EPS adapter
  -> PDU UART protocol
  -> SAME51 PDU firmware
  -> PDU hardware
```

The F Prime component should live in a separate flight-software repository. This
repository provides the PDU MCU firmware and the UART protocol that an adapter can
call.

## Current Protocol

The active interface is the framed binary UART v2 protocol documented in
[`PDU_PROTOCOL_ICD.md`](PDU_PROTOCOL_ICD.md).

Protocol features:

- fixed start-of-frame byte
- explicit protocol version and opcode
- sequence number echoed in responses
- payload length field
- fixed 9600 baud link setting
- CRC-16/CCITT over header and payload
- structured status codes
- inter-byte timeout for incomplete frames

Current commands:

| Command | Purpose |
| --- | --- |
| `PING` | Link check |
| `GET_PROTOCOL_INFO` | Version, capabilities, payload limits |
| `GET_SUMMARY_STATUS` | Output bitmap, reset cause, faults, uptime, capabilities |
| `GET_RESET_INFO` | MCU reset-cause register |
| `HELP` | Short human-readable command list for bench testing |
| `GET_OUTPUT_STATE` | Read one logical output or all logical outputs |
| `SET_OUTPUT_STATE` | Set one normal latchable output |
| `POWER_CYCLE_OUTPUT` | Turn an output off, wait, then restore it |
| `FIRE_BURN_WIRE` | Fire one burn-wire channel for a bounded duration |
| `SET_TORQUE_COIL` | Set one torque coil mode/current, optionally timed |
| `GET_TORQUE_COIL` | Read one torque coil state |
| `SOFTWARE_RESET` | Reply OK, then intentionally stop watchdog service |

## Repository Map

| Path | Purpose |
| --- | --- |
| `src/main.c` | Harmony entry point |
| `src/app.c`, `src/app.h` | Startup defaults and UART polling loop |
| `src/pdu_packet.c`, `src/pdu_packet.h` | Framed protocol parser, command handlers, GPIO behavior |
| `src/pdu_protocol_v2.h` | Shared in-repo wire-protocol constants for firmware and Teensy bench tooling |
| `PDU_PROTOCOL_ICD.md` | Protocol source of truth |
| `src/config/default/` | Harmony-generated configuration and drivers |
| `ArtemisPDU.X/` | MPLAB X project files |
| `teensy/` | Bench-test sketches for manual protocol and sensor checks |
| `docs/` | Architecture, hardware, and test documentation |

The Teensy comms sketch includes the same protocol header through a
sketch-local symlink, so active opcodes, status codes, output IDs, and payload
limits are defined in one place.

## Build

Use MPLAB X with the XC32 compiler and Harmony v3 project support.

This repository includes MPLAB X project files under `ArtemisPDU.X/`. Terminal
builds depend on the local Microchip toolchain being installed and configured.
Only claim a build passed if it was actually built with the local XC32/Harmony
environment.

## Testing

Bench testing currently uses the Teensy sketches under `teensy/`.

Start with:

- [`docs/teensy_testing.md`](docs/teensy_testing.md) for manual test commands
- [`PDU_PROTOCOL_ICD.md`](PDU_PROTOCOL_ICD.md) for packet layouts
- [`docs/current_pdu_architecture.md`](docs/current_pdu_architecture.md) for runtime behavior

Known practical checks:

- `ping`
- `info`
- `summary`
- `pdu-help`
- `get all`
- `set <output> <on|off>`
- `cycle <output> <off_ms>`

## Scope

Implemented now:

- framed UART command/response protocol
- logical output control and readback
- composite 12 V rail handling
- bounded power-cycle operations
- bounded burn-wire operations
- low-level torque-coil control/readback
- reset-cause and summary status reporting
- uptime based on FreeRTOS scheduler ticks
- parser recovery for truncated frames

Not implemented in this firmware:

- native USB/CDC command interface
- MicroSD logging or file operations
- full analog power, current, and temperature telemetry
- charger control/status abstraction
- asynchronous event frames

Those higher-level EPS telemetry and mission behaviors should be handled by the
Teensy/EPS adapter and F Prime component as appropriate.

## License

See the top-of-file headers and included third-party notices for license details.
