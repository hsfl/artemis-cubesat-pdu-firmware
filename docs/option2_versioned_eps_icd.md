# Option 2 Proposal: Versioned EPS ICD Over UART

## Summary

Option 2 proposes replacing the current loose ASCII-offset PDU command protocol
with a small, versioned EPS/PDU interface control document, implemented over
UART.

The recommendation is not to replace UART. UART is still a reasonable transport
between the Artemis bus Teensy 4.1 and the PDU MCU.

The recommendation is to replace the current command shape with a real protocol
boundary:

```text
Pi / F Prime mission layer
  -> Teensy 4.1 EPS client / supervisor
    -> versioned EPS UART ICD
      -> PDU firmware EPS logic
        -> PDU board driver
```

This keeps the same basic hardware relationship, but changes the software from
"send switch enum bytes and toggle GPIOs" to "send mission-level EPS requests
and receive structured EPS responses."

## Design Goal

The goal is to make the PDU interface easier for students to reason through and
safer to reuse across future smallsat and CubeSat missions.

The current protocol answers this question:

```text
Which enum and GPIO state did I send?
```

The proposed protocol should answer these questions:

```text
What EPS behavior did the mission request?
Was the request valid?
Was it accepted?
What did the PDU actually do?
What state or fault should the rest of the spacecraft know about?
```

That shift matters because future students should not have to learn board-level
pin coupling before they can command mission-level power behavior.

## Proposed Layering

Option 2 separates the system into four layers.

```text
Layer 1: Pi / F Prime
Layer 2: Artemis bus Teensy 4.1
Layer 3: Versioned EPS UART ICD
Layer 4: PDU MCU firmware and board driver
```

## Layer 1: Pi / F Prime Mission Layer

The Pi/F Prime layer should express mission intent.

Example responsibilities:

- request current EPS status
- command payload rail on or off
- sequence mode transitions
- report EPS telemetry to ground
- coordinate payload, comms, and power behavior

This layer should not know:

- which PDU GPIO pin enables a rail
- which rails are composite
- how the PDU serial frame is encoded
- whether a board revision uses one enable pin or two

In F Prime terms, this is where a stable `EpsService` belongs. It should expose
spacecraft-facing commands and telemetry. It should not become a copy of the
PDU GPIO driver.

## Layer 2: Artemis Bus Teensy 4.1

The Teensy 4.1 should act as the embedded EPS/PDU client and supervisor.

Example responsibilities:

- own the UART connection to the PDU
- send EPS protocol frames
- retry or time out requests
- monitor PDU link health
- provide a simple API to the Pi side
- optionally enforce local safety or watchdog behavior

The Teensy should not need to know the physical implementation of each rail. It
should call functions like:

```text
eps_ping()
eps_get_caps()
eps_set_rail(PAYLOAD_5V, ON)
eps_get_soh()
eps_get_faults()
eps_pulse_reset(RPI)
```

That client library becomes the student-facing integration surface on the bus
side.

## Layer 3: Versioned EPS UART ICD

The UART ICD is the contract between the bus Teensy and the PDU MCU.

A compact framed packet is enough:

```text
magic | version | msg_type | sequence | payload_length | payload | crc
```

One possible byte-level shape:

```text
u8  magic_0
u8  magic_1
u8  version
u8  msg_type
u8  sequence
u8  payload_length
u8  payload[payload_length]
u16 crc
```

This is still small enough for microcontrollers, but it adds the missing pieces
from the current architecture:

- a recognizable frame start
- an explicit protocol version
- a message type
- a sequence number for request/response matching
- a payload length
- a CRC for corruption detection
- room for structured payloads

The exact field sizes can be adjusted, but the design intent should stay the
same: every frame should be self-describing enough to validate before it affects
hardware.

## Layer 4: PDU MCU Firmware

The PDU MCU should own hardware truth.

It should know:

- which GPIOs control each rail
- which outputs are composite
- which state transitions are allowed
- which sensors prove state
- which faults are latched
- which startup defaults are safe
- how long a pulse or delay should last

The PDU firmware should expose those details through logical EPS behavior, not
raw pin knowledge.

For example, `SET_RAIL SW_12V ON` should internally know that the 12 V rail
requires the 5 V source path. The caller should not need to issue two separate
pin commands unless the mission explicitly wants that low-level test mode.

## Proposed Command Set

The first version should stay intentionally small.

Recommended v2 command set:

```text
PING
GET_CAPS
SET_RAIL
GET_RAIL
GET_SOH
GET_FAULTS
PULSE_RESET
ARM_BURN
FIRE_BURN
DISARM_BURN
```

Optional later commands:

```text
GET_BOARD_INFO
GET_ANALOG_TELEM
SET_HEATER
GET_HEATER
SET_CHARGER_MODE
GET_CHARGER_STATUS
SET_TORQUE
GET_TORQUE_STATUS
CLEAR_FAULT
ENTER_SAFE_DEFAULTS
```

The first version should avoid adding every possible feature. A small reliable
protocol is easier to test, teach, and extend than a broad partially
implemented protocol.

## Proposed Response Model

Every command that changes hardware should receive a structured response.

Recommended response fields:

```text
sequence
status
error_code
optional payload
```

Example status values:

```text
OK
REJECTED
BAD_CRC
BAD_LENGTH
UNSUPPORTED_VERSION
UNSUPPORTED_COMMAND
INVALID_RAIL
UNSAFE_STATE
BUSY
HARDWARE_FAULT
```

This is one of the largest improvements over the current architecture. Today,
an invalid or unsupported command can silently fall through. In a mission-style
interface, the caller should know whether the PDU accepted, rejected, ignored,
or failed a request.

## Rail Abstraction

The v2 protocol should use logical rail identifiers instead of exposing raw GPIO
details.

Example logical rails:

```text
RAIL_3V3_1
RAIL_3V3_2
RAIL_5V_1
RAIL_5V_2
RAIL_5V_3
RAIL_5V_4
RAIL_12V
RAIL_VBATT
RAIL_RPI
RAIL_PAYLOAD
```

The exact names should match the board and mission vocabulary. The key decision
is that the protocol should describe logical power behavior, not individual MCU
pin names.

This enables future board revisions. A new PDU board can keep the same logical
rail ID while changing the internal pin mapping.

## State-of-Health Telemetry

`GET_SOH` should return a compact summary of EPS health.

Recommended first fields:

```text
protocol_version
firmware_version
uptime_ms
rail_state_bitmap
fault_bitmap
reset_reason
last_error
```

Later fields can include:

```text
battery_voltage
battery_current
bus_3v3_voltage
bus_5v_voltage
bus_12v_voltage
temperature
charger_state
heater_state
watchdog_state
```

The tradeoff is payload size. A compact SOH packet should fit normal health
polling. Larger analog telemetry can be split into a separate command if needed.

## Capability Discovery

`GET_CAPS` should report what the PDU firmware and board revision actually
support.

Example capability fields:

```text
protocol_version_min
protocol_version_max
board_id
firmware_version
supported_command_bitmap
supported_rail_bitmap
supported_sensor_bitmap
```

This helps future missions because the bus Teensy or Pi can detect whether it
is talking to the expected PDU behavior before commanding hardware.

Capability discovery is especially useful for student teams because it reduces
hidden assumptions. A test script can fail early with "this board does not
support payload rail control" instead of timing out or sending the wrong enum.

## Firmware Refactor Shape

The PDU firmware should be refactored into four modules:

```text
pdu_transport
  -> UART receive/transmit, frame sync, timeout handling

pdu_protocol
  -> frame decode, CRC check, message encode, version handling

pdu_eps
  -> command dispatch, safety rules, rail state model, SOH construction

pdu_board
  -> GPIO macros, sensor reads, composite rail implementation
```

This keeps the teaching path clean:

```text
Transport answers: did bytes arrive correctly?
Protocol answers: is the request valid?
EPS logic answers: is the requested behavior allowed?
Board driver answers: which pins and sensors implement it?
```

That is easier to debug than a single handler that does all four things.

## Teensy 4.1 Client Library

The bus Teensy should use a small client library instead of open-coded packet
construction.

Recommended shape:

```text
eps_client.hpp
eps_client.cpp
```

Example API:

```c++
bool epsPing(EpsVersion* version);
bool epsGetCaps(EpsCaps* caps);
bool epsSetRail(EpsRail rail, bool enabled, EpsAck* ack);
bool epsGetRail(EpsRail rail, EpsRailState* state);
bool epsGetSoh(EpsSoh* soh);
bool epsPulseReset(EpsResetTarget target, uint16_t durationMs);
```

The client should own:

- sequence number allocation
- frame encoding
- CRC calculation
- response timeout
- response matching
- basic retries if desired

This gives student users a stable surface. They can call functions and inspect
clear return values without hand-building frames every time.

## F Prime Boundary

The F Prime side should remain mission-facing.

Recommended boundary:

```text
EpsService
  -> stable mission commands and telemetry

EpsAdapter_Artemis
  -> platform-specific bridge to the Artemis bus Teensy / PDU client

Teensy EPS client
  -> UART protocol implementation

PDU firmware
  -> hardware control and telemetry
```

This prevents F Prime components from becoming dependent on PDU board pins or
on the exact serial frame layout.

That distinction is important for future reuse. A different EPS board should
require a new adapter or client backend, not a rewrite of mission-level F Prime
logic.

## Safety Decisions

Option 2 should add explicit safety behavior.

Recommended rules:

- reject malformed frames before command dispatch
- reject unsupported protocol versions
- reject unknown command IDs
- reject invalid rail IDs
- require arm/fire separation for burn-wire commands
- make pulse-duration commands bounded
- return structured error codes
- expose fault state through `GET_FAULTS` and `GET_SOH`
- keep board-specific default states inside the PDU firmware

The burn-wire path deserves special treatment. It should not be just another
generic `SET_RAIL` command. It should require an explicit arm command, a bounded
fire command, and an automatic disarm or timeout behavior.

## Why Not Keep the Current ASCII Protocol?

Keeping the ASCII protocol is the fastest path.

It is good for:

- visible serial debugging
- simple student demos
- quick rail testing
- low implementation effort

But it has weak mission properties:

- no frame version
- no CRC
- no structured ACK/NACK
- no payload length
- no sequence number
- unclear behavior for malformed input
- raw enum values become the interface
- board-specific switch behavior leaks upward

If the goal is only to control the current board in the lab, the ASCII protocol
can remain as legacy v1. If the goal is a reusable EPS/PDU interface for future
missions, it should not be the main architecture.

## Why Not Jump Straight to a Generated Model?

A generated model-driven interface is attractive. A YAML or JSON source of truth
could generate:

- PDU rail tables
- Teensy client constants
- Python test tools
- F Prime adapter constants
- Markdown ICD docs

That is a good long-term direction.

The tradeoff is complexity. Generators are only useful once the interface is
stable enough to generate. If introduced too early, students may spend more time
debugging the generator than understanding the EPS behavior.

Option 2 is the middle step:

```text
first make the boundary real
then make it generated later
```

## Migration Plan

Recommended migration path:

1. Document current protocol as legacy v1.
2. Add a v2 protocol header and frame parser beside the current decoder.
3. Implement `PING` and `GET_CAPS`.
4. Implement `SET_RAIL` and `GET_RAIL` using a rail table.
5. Implement `GET_SOH` with compact status fields.
6. Add a Teensy 4.1 client library.
7. Add a simple host-side test tool for serial verification.
8. Connect the F Prime EPS adapter to the Teensy-side client path.
9. Keep legacy v1 temporarily for board bring-up and fallback testing.
10. Retire v1 once v2 has hardware proof.

This avoids a risky rewrite. The team can keep the existing demo path alive
while building the better interface beside it.

## Validation Strategy

Validation should happen in layers.

Transport validation:

- bad magic rejected
- bad length rejected
- bad CRC rejected
- partial frame does not execute hardware

Protocol validation:

- unsupported version rejected
- unknown command rejected
- sequence number echoed in response
- malformed payload rejected

EPS behavior validation:

- valid rail command changes the expected state
- invalid rail command is rejected
- composite rail behavior is handled internally
- burn-wire command requires arm state
- reset pulse duration is bounded

Hardware validation:

- GPIO state matches expected rail behavior
- composite rails report correct readback
- SOH updates after rail changes
- fault bits are reported consistently

System validation:

- Teensy client can ping PDU
- Teensy client can set and read a rail
- Pi/F Prime layer can request EPS status through the adapter
- command response is visible from ground telemetry

## Engineering Tradeoffs

### More code, but cleaner reasoning

Option 2 adds more code than the current decoder. That is the cost of explicit
boundaries.

The benefit is that students can debug one layer at a time. A CRC failure is a
transport/protocol issue. A rejected rail is an EPS logic issue. A wrong pin
state is a board-driver issue.

### Less convenient serial typing, but better automation

The current ASCII protocol is easy to type into a serial terminal. A framed
binary protocol is less convenient by hand.

The answer is not to design the mission protocol around manual typing. The
answer is to provide small tools:

- Teensy client library
- host-side Python serial CLI
- scripted smoke tests

Manual serial testing can still exist through a debug shell or a legacy v1
compatibility mode.

### Slightly larger packets, but stronger fault detection

Adding magic, version, length, sequence, and CRC increases packet size.

For PDU/EPS commands, that cost is acceptable. These are low-rate control and
status messages, not high-bandwidth payload data. The reliability and clarity
benefit is worth the extra bytes.

### Stronger abstraction, but less direct pin access

Mission software should not directly command arbitrary GPIO pins. That is a
feature, not a bug.

If low-level pin testing is needed, it can live behind a debug-only command set
or compile-time test mode. The normal mission ICD should expose safe EPS
behavior.

## Recommended Decision

Adopt Option 2 as the next architecture.

Keep the current ASCII protocol documented as legacy v1 for bring-up and
fallback testing. Build a versioned v2 EPS ICD beside it. Start with the minimum
useful command set:

```text
PING
GET_CAPS
SET_RAIL
GET_RAIL
GET_SOH
GET_FAULTS
```

Then add safety-sensitive actions like reset pulses and burn-wire control only
after the frame parser, ACK/NACK model, and Teensy client are proven.

The desired end state is:

```text
students reason about EPS behavior
protocol tools handle bytes
PDU firmware owns hardware details
F Prime owns mission behavior
```

That architecture is clear enough to teach and structured enough to reuse.
