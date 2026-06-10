# Teensy/PDU UART Test Script Plan

## Goal

Create a small host-side Python script that manually tests the PDU v2 framed UART protocol through the Teensy/PDU UART path. This is a bench tool, not flight software.

The script should let an operator send individual commands, inspect framed responses, and verify rails, burn-wire commands, torque-coil commands, reset info, and parser behavior.

Note: a student-friendly Teensy manual test sketch now exists at
`teensy/pdu_comms_test/pdu_comms_test.ino`. Build and use that first for direct
Teensy-to-PDU checkout. This Python tool is still useful later for host-side
automation and scripted regression tests.

## Constraints

- Do not modify firmware.
- Use the active protocol in `PDU_PROTOCOL_ICD.md`.
- Keep the tool simple and manual.
- Use Python 3.
- Prefer `pyserial`.
- Do not require a GUI.
- Do not auto-fire burn wires on startup.
- Burn-wire commands must require an explicit CLI command and visible confirmation text.

## Suggested File

Create:

```text
tools/pdu_uart_cli.py
```

If `tools/` does not exist, create it.

## Protocol Basics

Frame:

```text
SOF 0xA5 | version | msg_type | opcode | seq | status | payload_len | payload | crc16_le
```

Constants:

```text
VERSION = 2
SOF = 0xA5
MSG_REQUEST = 0
MSG_RESPONSE = 1
MAX_PAYLOAD_LEN = 96
```

CRC:

- CRC-16/CCITT
- Initial value `0xFFFF`
- Polynomial `0x1021`
- Covers bytes from `version` through final payload byte
- Does not cover SOF
- Sent little-endian

## Required CLI Shape

Use subcommands:

```text
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX ping
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX info
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX summary
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX reset-info
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX get-output 0x03
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX set-output 0x03 1
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX power-cycle 0x03 1000
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX fire-burn 1 500 --i-understand
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX set-torque 1 forward 100 --duration-ms 250
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX get-torque 1
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX raw 10 03
```

Default serial settings should be configurable:

```text
--baud 9600
--timeout 1.0
--seq 0
--hex
--verbose
```

The current generated PDU UART configuration is `9600` baud. Keep `--baud`
easy to override in case the Harmony UART config changes later.

## Required Commands

### `ping`

Opcode `0x00`, no payload.

Expected response payload:

```text
byte 0 = protocol version
```

### `info`

Opcode `0x01`, no payload.

Decode:

```text
version
capability bitmap
max payload length
output count
firmware major/minor/patch
```

### `summary`

Opcode `0x02`, no payload.

Decode:

```text
output bitmap
reset cause
fault bitmap
uptime seconds
capability bitmap
```

Print named output bits:

```text
bit 0 = 3V3_1
bit 1 = 3V3_2
bit 2 = 5V_1
bit 3 = 5V_2
bit 4 = 5V_3
bit 5 = 12V
bit 6 = VBATT
bit 7 = BURN1
bit 8 = BURN2
```

Print fault bits:

```text
bit 0 = HBRIDGE1 fault
bit 1 = HBRIDGE2 fault
```

### `reset-info`

Opcode `0x03`, no payload.

Decode known reset cause bits from ICD.

### `get-output OUTPUT_ID`

Opcode `0x10`.

Payload:

```text
byte 0 = output ID, or 0xFF for all
```

Print output name and state.

### `set-output OUTPUT_ID STATE`

Opcode `0x11`.

Payload:

```text
byte 0 = output ID
byte 1 = state, 0 or 1
```

Do not allow burn or H-bridge reserved IDs here unless user passes a force flag. Prefer no force flag for MVP.

### `power-cycle OUTPUT_ID OFF_MS`

Opcode `0x12`.

Payload:

```text
byte 0 = output ID
byte 1..2 = off time ms little-endian
```

Print that this command is nonblocking and response state should initially be off.

### `fire-burn CHANNEL DURATION_MS --i-understand`

Opcode `0x13`.

Map:

```text
channel 1 -> PDU_OUTPUT_BURN1 = 0x09
channel 2 -> PDU_OUTPUT_BURN2 = 0x0A
```

Payload:

```text
byte 0 = output ID
byte 1..2 = duration ms little-endian
byte 3..4 = arm token 0xB142 little-endian
```

Require `--i-understand`. If absent, print a warning and exit without sending.

### `set-torque COIL MODE CURRENT`

Opcode `0x14`.

Arguments:

```text
COIL = 1..4
MODE = coast|forward|reverse|brake
CURRENT = 100|50
--duration-ms N, default 0
```

Payload:

```text
byte 0 = coil ID
byte 1 = mode, 0 coast, 1 forward, 2 reverse, 3 brake
byte 2 = current, 0 for 100%, 1 for 50%
byte 3..4 = duration ms little-endian
```

Decode response:

```text
coil ID
mode
current scalar
driver awake state
fault active
```

### `get-torque COIL`

Opcode `0x15`.

Payload:

```text
byte 0 = coil ID
```

Decode same response as `set-torque`.

### `raw OPCODE [PAYLOAD_HEX...]`

Manual escape hatch for bench testing.

Example:

```text
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX raw 00
python tools/pdu_uart_cli.py --port /dev/tty.usbmodemXXXX raw 10 ff
```

## Response Handling

Implement a `read_frame(serial, timeout)` function:

1. Read until SOF `0xA5`.
2. Read the remaining fixed header bytes.
3. Read `payload_len + 2` bytes.
4. Validate CRC.
5. Validate version.
6. Return a parsed response object.

Important: after SOF, do not resync on `0xA5`; it may appear in payload or CRC.

Print:

```text
rx frame hex
status name
opcode name
seq
payload hex
decoded payload
```

## Status Names

Decode:

```text
0 = OK
1 = BAD_OPCODE
2 = BAD_LENGTH
3 = BAD_PARAM
4 = HW_FAULT
5 = NOT_IMPLEMENTED
6 = BUSY
```

## Manual Bench Flow

Recommended first run:

```text
ping
info
summary
get-output 0xFF
set-output 0x03 1
get-output 0x03
power-cycle 0x03 1000
summary
set-torque 1 forward 100 --duration-ms 250
get-torque 1
set-torque 1 coast 100
```

Burn-wire tests should only happen with the hardware configured for safe bench testing:

```text
fire-burn 1 500 --i-understand
fire-burn 2 500 --i-understand
```

Parser robustness test:

- Send a valid frame whose payload contains `0xA5`.
- Good target: `raw 04` is not payload-bearing, so instead use `raw 10 a5` and expect `BAD_PARAM` with a valid response, proving parser did not restart just because payload was `0xA5`.

## Acceptance Criteria

- Script can build and send valid CRC frames.
- Script can parse and validate response frames.
- `ping`, `info`, `summary`, `get-output`, `set-output`, `power-cycle`, `fire-burn`, `set-torque`, `get-torque`, and `raw` exist.
- Burn command refuses to run without `--i-understand`.
- Output decoding names match `PDU_PROTOCOL_ICD.md`.
- Script does not require firmware changes.
