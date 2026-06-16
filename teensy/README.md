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

- `pdu_all_test/pdu_all_test.ino`
  - Operator-driven full bench checkout profile for link, protocol info,
    status, output readback, selected safe rail operations, charger status, and
    torque readback.
- `pdu_vibration_test/pdu_vibration_test.ino`
  - Autonomous vibration-test profile that commands outputs off, shuts the
    charger down, coasts torque coils, and logs periodic PDU status to Teensy SD.
- `pdu_thermal_test/pdu_thermal_test.ino`
  - Autonomous thermal-vac support profile that applies an editable rail policy
    and logs PDU status plus TMP36/INA219 telemetry to Teensy SD.

- `pdu_test_common/pdu_test_common.h`
  - Shared framed-UART client used by the profile sketches. It includes
    `src/pdu_protocol_v2.h` directly so active protocol constants stay in the
    firmware repo source of truth.

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
charger?
charger on
charger off
burn burn1 1000 arm
reset-pdu arm
```

Burn-wire and reset commands require the literal `arm` word so they cannot be
triggered by a casual typo during bench testing.

## Profile Sketches

Use `pdu_all_test` for local bench work where a person can type commands and
measure rails. Start with `run`, then use `set <output> <on|off>`, `cycle`,
`summary`, `reset-info`, `charger?`, `charger <on|off>`, and guarded torque
commands as needed.

Use `pdu_vibration_test` when no one will have console access. On boot it forces
outputs off, charger shutdown, and torque coils coast/off. It creates
`VIBE00.CSV`, `VIBE01.CSV`, etc. on the Teensy SD card when available and still
mirrors logs to USB Serial if connected.

Use `pdu_thermal_test` for thermal-vac support. Edit `THERMAL_OUTPUT_PLAN` and
`THERMAL_ENABLE_CHARGER` in the sketch before the run. It creates `TVAC00.CSV`,
`TVAC01.CSV`, etc. and logs PDU summary/output/charger state plus TMP36 and
INA219 sensor readings.

## Charger Scope

The comms sketch supports `charger?` and `charger <on|off>`. Use the LTC4012
datasheet polarity: `SHDN` high enables the charger, `SHDN` low shuts it down,
and `CHRG` is an active-low open-drain charge indicator. `charger?` reports the
`SHDN` output latch and raw `CHRG` input state.

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
