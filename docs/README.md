# Artemis PDU Documentation

This folder collects the practical documentation for the Artemis CubeSat PDU
firmware, hardware interface, and bench-test tooling.

## Start Here

Read these in order when onboarding to the PDU:

1. [Repository README](../README.md)
   - Public overview, current functionality, build notes, and system role.
2. [PDU Hardware Reference](pdu_hardware_reference.md)
   - Board-level purpose, connectors, rails, switches, sensors, burn wires,
     H-bridges, battery charging, and safety circuits.
3. [Current PDU Firmware Architecture](current_pdu_architecture.md)
   - Runtime path, boot behavior, UART parser, logical outputs, burn-wire
     handling, torque-coil handling, and removed legacy code.
4. [PDU Protocol ICD](../PDU_PROTOCOL_ICD.md)
   - Source of truth for the active framed v2 UART protocol.
5. [Teensy PDU Testing Guide](teensy_testing.md)
   - How to use the new Teensy comms and sensor-check sketches.

## Design And Planning Docs

- [Option 2: Versioned EPS ICD Over UART](option2_versioned_eps_icd.md)
  - Original design direction for moving away from legacy ASCII commands.
- [Teensy/PDU UART Test Script Plan](teensy_pdu_uart_test_script_plan.md)
  - Host-side Python CLI plan for future bench automation.

## Related Repo Paths

- Active protocol constants: `src/pdu_protocol_v2.h`
- Active protocol parser/handlers: `src/pdu_packet.c`
- App UART ingress path: `src/app.c`
- Teensy comms sketch: `teensy/pdu_comms_test/pdu_comms_test.ino`
- Teensy sensor sketch: `teensy/pdu_board_sensor_test/pdu_board_sensor_test.ino`

## Current Scope Notes

- USB-C hardware exists, but native USB/CDC firmware is not implemented.
- MicroSD hardware and generated Harmony SD/FATFS support exist, but the
  handwritten app runtime does not currently use MicroSD.
- The active command interface is framed binary UART v2.
- The old ASCII/newline PDU command format is legacy reference only.
- Current firmware supports link checks, protocol info, summary status, reset
  info, output get/set, power cycle, burn-wire pulse, torque-coil control,
  software-reset request, and bench help.
- The PDU firmware exposes low-level PDU control/status. The Teensy/EPS adapter
  is expected to combine this with board telemetry for the F Prime EPS view.
