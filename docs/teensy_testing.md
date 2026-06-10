# Teensy PDU Testing Guide

## Purpose

The Teensy sketches provide simple bench tools for students to validate the PDU
without reusing the old legacy ASCII/newline command format.

Use:

- `teensy/pdu_comms_test/pdu_comms_test.ino` for PDU MCU UART protocol testing
- `teensy/pdu_board_sensor_test/pdu_board_sensor_test.ino` for direct sensor
  line checkout

## PDU Comms Test

Sketch:

```text
teensy/pdu_comms_test/pdu_comms_test.ino
```

Default ports:

- USB Serial Monitor: `Serial`
- PDU UART: `Serial1`
- baud: `9600`

Wire the link as:

- Teensy `Serial1 TX` to PDU UART RX
- Teensy `Serial1 RX` to PDU UART TX
- Teensy GND to PDU GND

The sketch implements the active framed v2 UART protocol:

```text
SOF 0xA5 | version | msg_type | opcode | seq | status | payload_len | payload | crc16
```

It includes:

- CRC-16/CCITT frame generation
- response CRC validation
- sequence matching
- readable command wrappers
- a simple Arduino Serial Monitor command parser

Manual commands:

```text
help
pdu-help
ping
info
summary
reset-info
outputs
get <output|all>
set <output> <on|off>
cycle <output> <off_ms>
torque <coil 1-4> <coast|forward|reverse|brake> <100|50> [duration_ms]
torque? <coil 1-4>
burn <burn1|burn2> <duration_ms> arm
reset-pdu arm
```

Recommended first bring-up sequence:

```text
ping
info
summary
get all
set 5v1 on
get 5v1
cycle 5v1 500
get 5v1
torque? 1
torque 1 forward 100 250
torque? 1
```

Do not test burn-wire commands with flight deployment hardware connected unless
the mechanical and safety setup is explicitly ready for a burn test.

## Sensor Test

Sketch:

```text
teensy/pdu_board_sensor_test/pdu_board_sensor_test.ino
```

This sketch checks:

- TMP36 analog temperature input on `A1`
- INA219 sensors on `Wire2`

Expected INA219 addresses:

| Address | Sensor |
|---------|--------|
| `0x40` | Solar panel input 1 |
| `0x41` | Solar panel input 2 |
| `0x42` | Solar panel input 3 |
| `0x43` | Solar panel input 4 |
| `0x44` | VBatt / battery bus |

The sketch prints:

- TMP36 raw ADC count
- TMP36 voltage
- calculated temperature
- INA219 bus voltage
- INA219 shunt voltage
- INA219 current
- INA219 power

This sensor sketch does not talk to the PDU MCU. It is a direct electrical
checkout tool.

## Flight-Software Handoff

For F Prime or flight-ready Teensy work, reuse only the protocol core from the
manual comms sketch:

- constants
- CRC function
- frame encoder
- response reader
- typed command wrappers

Do not reuse the Arduino Serial Monitor parser as flight code. In flight
software, replace manual text commands with typed component commands, ports,
telemetry channels, and events.
