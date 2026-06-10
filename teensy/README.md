# Teensy PDU Test Sketches

These sketches are for bench testing the Artemis PDU with a Teensy.

## Sketches

- `pdu_comms_test/pdu_comms_test.ino`
  - Talks to the PDU MCU over the new framed v2 UART protocol.
  - Use this for manual protocol testing through the Arduino Serial Monitor.
  - This replaces the old legacy ASCII/newline `pdu_comm.ino` approach.

- `pdu_board_sensor_test/pdu_board_sensor_test.ino`
  - Checks the PDU-side analog temperature input and INA219 current sensors.
  - This is not the PDU MCU command protocol. It is a separate board-health test.

## UART Wiring

Default PDU UART settings:

- Baud: `9600`
- Teensy console: USB `Serial`
- Teensy to PDU: `Serial1`

Wire the UART as crossed TX/RX:

- Teensy `Serial1 TX` -> PDU UART RX
- Teensy `Serial1 RX` -> PDU UART TX
- Teensy GND -> PDU GND

## Manual Comms Commands

Open the Arduino Serial Monitor at `9600` baud and send commands like:

```text
help
ping
info
summary
outputs
get all
get 5v1
set 5v1 on
set 5v1 off
cycle 5v1 500
torque 1 forward 100 250
torque? 1
burn burn1 1000 arm
reset-pdu arm
```

Burn-wire and reset commands require the literal `arm` word so they cannot be
triggered by a casual typo during bench testing.

## Flight-Software Direction

For future F Prime integration, lift the protocol-core pieces from
`pdu_comms_test.ino`:

- protocol constants
- `crc16Ccitt()`
- `sendRequest()`
- `readResponse()`
- small command wrappers

Do not carry over the Arduino Serial Monitor console parser into flight code.
That parser is only for student-friendly manual testing.
