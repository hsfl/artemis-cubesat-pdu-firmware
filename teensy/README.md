# Teensy PDU Test Sketches

These sketches are for bench testing the Artemis PDU with a Teensy.

## Sketches

- `pdu_comms_test/pdu_comms_test.ino`
  - Talks to the PDU MCU over the new framed v2 UART protocol.
  - Use this for manual protocol testing through the Arduino Serial Monitor.
  - This replaces the old legacy ASCII/newline `pdu_comm.ino` approach.
  - Includes `src/pdu_protocol_v2.h` through a sketch-local symlink so protocol constants are not duplicated.

- `pdu_board_sensor_test/pdu_board_sensor_test.ino`
  - Checks the PDU-side analog temperature input and INA219 current sensors.
  - This is not the PDU MCU command protocol. It is a separate board-health test.

Planned next sketches:

- `pdu_all_test/pdu_all_test.ino`
  - Bench checkout profile for link, protocol info, status, output readback,
    selected safe rail operations, and torque readback.
- `pdu_vibe_test/pdu_vibe_test.ino`
  - Vibration-test setup profile that commands normal outputs off and verifies
    burn-wire and torque-coil safe states.
- `pdu_thermal_test/pdu_thermal_test.ino`
  - Thermal-vac support profile that polls PDU status and enables only the rails
    required by the approved thermal test setup.

Keep these as Teensy-side profiles rather than PDU firmware modes. The PDU
firmware should remain a low-level EPS controller with explicit commands and
readbacks.

## UART Wiring

Default PDU UART settings:

- Fixed baud: `9600`
- Teensy console: USB `Serial`
- Teensy to PDU: `Serial1`

The comms sketch does not allow changing the PDU UART baud at runtime. This
keeps manual bench testing from accidentally desynchronizing the Teensy and PDU.

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

## Charger Scope

Do not add charger commands to the Teensy tester yet. The hardware manual names
`SHDN` and `CHRG`, but this repo's generated MPLAB/Harmony pin configuration
does not currently expose confirmed named MCU pins for those signals. Charger
test support should wait until that pin mapping is verified and generated
configuration is updated deliberately.

When charger support is added, use the LTC4012 datasheet polarity: `SHDN` high
enables the charger, `SHDN` low shuts it down, and `CHRG` is an active-low
open-drain charge indicator.

## Flight-Software Direction

For future F Prime integration, lift the protocol-core pieces from
`pdu_comms_test.ino`:

- `src/pdu_protocol_v2.h` as the shared protocol contract
- `crc16Ccitt()`
- `sendRequest()`
- `readResponse()`
- small command wrappers

Do not carry over the Arduino Serial Monitor console parser into flight code.
That parser is only for student-friendly manual testing.
