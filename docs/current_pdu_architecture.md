# Current PDU Firmware Architecture

## Summary

The Artemis PDU firmware now uses a framed binary UART protocol to control
logical power outputs and return structured status responses.

The active behavior is based on the repo protocol ICD and the Artemis PDU
manual. The temporary student PDF ICD should be ignored for current firmware
scope except as rough historical intent.

At a high level, the system is:

```text
External controller / Artemis bus Teensy 4.1
  -> framed UART request
    -> ATSAME51 PDU firmware
      -> validated protocol handler
      -> logical GPIO output control
      -> framed UART response
```

The handwritten runtime path is intentionally small so future students can
trace the active behavior in a few files.

## Main Runtime Path

```text
src/main.c
  -> SYS_Initialize(NULL)
  -> SYS_Tasks()
    -> FreeRTOS lAPP_Tasks()
      -> APP_Tasks()
        -> USART_READ()
          -> pdu_protocol_process_byte()
            -> pdu_handle_request()
              -> GPIO state read/write or status response
```

The important handwritten files are:

- `src/app.c`
  - applies safe startup GPIO defaults through `disableAllGPIOs()`
  - leaves the SK6812 LED/DIN line idle-low
  - polls SERCOM3 UART in bounded byte batches
  - resets parser state on UART read/error failure
- `src/pdu_packet.c`
  - owns framed protocol parsing
  - validates CRC, version, message type, payload length, and parameters
  - implements command handlers
  - maps logical output IDs to board GPIO behavior
  - sends framed responses
- `src/pdu_protocol_v2.h`
  - defines the active v2 wire protocol constants, opcodes, output IDs, status
    codes, payload sizes, and capability bits
- `PDU_PROTOCOL_ICD.md`
  - documents the active wire protocol for controller-side implementation

The generated Harmony and FreeRTOS code remains under `src/config/default/`.
That layer still creates the SYS_FS, SD SPI, watchdog, and app tasks. SD/FATFS
support remains configured for future use, but the handwritten app no longer
mounts or writes to SD during normal runtime.

## USB-C and MicroSD Hardware Notes

The PDU board manual describes a USB-C connector that carries both native USB
signals (`USB_D+`/`USB_D-`) and `UART2_RXD`/`UART2_TXD`. In this firmware, the
active command interface is the UART path on SERCOM3, matching the `UART2`
signals. The Artemis bus Teensy is still the primary controller-side UART
master, so the USB-C UART path should be treated as a bench/debug/service path
unless the electrical design intentionally arbitrates access between the Teensy
and a host connected through USB-C.

Native USB over `USB_D+`/`USB_D-` is not implemented by the current firmware.
There is no Harmony USB device stack, USB CDC interface, DFU path, or USB
bootloader integration in the active runtime.

The MicroSD connector is also present in hardware, and generated Harmony
SDSPI/FATFS support remains in the project. The current handwritten app does
not mount the card, log telemetry, load configuration, or perform firmware
update actions from MicroSD. Reasonable future uses include telemetry/event
logging, stored test scripts, persistent configuration, or offline debug data.

## Boot Behavior

On startup, `APP_Initialize()` calls `disableAllGPIOs()`.

That helper is the shared all-off path used by boot and protocol-level safe
defaults. It clears rail, burn, and H-bridge control pins, including
`BURN1_EN` and `BURN_5V`.

After GPIO defaults are applied, the firmware keeps the `LED` net low. The
installed PCB part is an SK6812 smart RGB LED, so PA21 drives the LED's `DIN`
input rather than a simple LED anode/cathode. A static high level does not turn
the LED on; it needs a timing-specific SK6812/NeoPixel data frame. Because the
PCB is already locked and the indicator is not worth the added maintenance cost,
the firmware intentionally leaves this line idle-low and does not implement an
SK6812 driver.

RTC and other generated peripherals are initialized by `SYS_Initialize()` before
`APP_Initialize()` runs.

The application task then continuously polls the command UART.

## UART Receive Behavior

The firmware uses SERCOM3 as the active command UART.

`USART_READ()` drains a bounded batch of UART bytes when the receiver is ready
and no USART error is reported. If the generated USART read call succeeds, each
byte is passed to `pdu_protocol_process_byte()`.

If a read fails or a USART error is detected, the protocol parser is reset so a
corrupted partial frame cannot affect the next valid frame.

If a frame starts but the next byte does not arrive within 100 ms, the parser
also discards that incomplete frame and waits for a fresh SOF byte.

## Active Wire Protocol

The active protocol is a small framed binary request/response protocol:

```text
sof | version | msg_type | opcode | seq | status | payload_len | payload | crc16
```

Key properties:

- SOF byte: `0xA5`
- protocol version: `2`
- request and response message types
- sequence number echoed in the response
- explicit payload length
- CRC-16/CCITT over the frame except SOF
- little-endian multi-byte payload fields
- fixed-width fields only
- 100 ms inter-byte timeout for incomplete frames

After SOF, the parser follows the explicit payload length, so `0xA5` is allowed
inside payload and CRC bytes.

`PDU_PROTOCOL_ICD.md` is the source of truth for byte layout and payloads.

## MVP Commands

The current firmware implements:

- `PING`
- `GET_PROTOCOL_INFO`
- `GET_SUMMARY_STATUS`
- `GET_RESET_INFO`
- `HELP`
- `GET_OUTPUT_STATE`
- `SET_OUTPUT_STATE`
- `POWER_CYCLE_OUTPUT`
- `FIRE_BURN_WIRE`
- `SET_TORQUE_COIL`
- `GET_TORQUE_COIL`
- `GET_CHARGER_STATUS`
- `SET_CHARGER_STATE`
- `SOFTWARE_RESET`

`GET_OUTPUT_STATE` supports `PDU_OUTPUT_ALL` for all-output readback.
`SET_OUTPUT_STATE` rejects `PDU_OUTPUT_ALL` so there is no one-command
enable-all path.
Burn-wire outputs are also rejected by `SET_OUTPUT_STATE` so deployment
channels cannot be latched on through generic rail control.

`POWER_CYCLE_OUTPUT` is limited to normal rails and VBATT that are already on.
It rejects `ALL`, the reserved `5V_4` input, burn outputs, and H-bridge
outputs. The command is nonblocking: it starts the off interval, replies, and
the app task service loop restores the rail later.

`FIRE_BURN_WIRE` is the only command path for burn-wire activation. It requires
a burn output ID, bounded duration, and arm token, then forces the selected
burn channel off after the requested duration.

`SET_TORQUE_COIL` and `GET_TORQUE_COIL` expose low-level torque-coil control
for the OBC/Teensy ADCS controller. The PDU does not decide attitude-control
timing or pointing logic; it only maps coil ID, direction mode, current scalar,
and optional duration to the DRV8847 control pins.

`SOFTWARE_RESET` replies `OK`, then asks the watchdog task to stop servicing the
watchdogs so the board resets through the hardware watchdog path.

The firmware intentionally does not implement `ALL`, `VIBE`, or `THERMAL` as
on-board modes. Those are bench/test intents and should live as separate
Teensy-side test sketches so the operator can inspect each step and avoid
one-command broad state changes in flight firmware.

Battery-charger commands use the generated physical pin 36 `PB14` as `SHDN` and
physical pin 70 `PA20` as `CHRG`.

Per the LTC4012 datasheet, `SHDN` is active-low shutdown: `SHDN_Clear()` should
mean charger disabled/shutdown, and `SHDN_Set()` should mean charger enabled.
`CHRG` is an active-low open-drain charge indicator, so a low read means the
charge indicator is active, subject to the board pull-up and weak-pulldown
behavior.

`GET_CHARGER_STATUS` reads interpreted charger state, the `SHDN` output latch,
and the raw `CHRG` input state.
`SET_CHARGER_STATE` accepts `1=enable` or `0=shutdown`, drives `SHDN`, then
returns the same status payload as `GET_CHARGER_STATUS`.

## Logical Outputs

The protocol exposes logical outputs, not raw GPIO commands.

Important composite outputs:

- `PDU_OUTPUT_12V`
  - enables or reads both `SW_5V_EN4` and `SW_12V_EN1`
  - `SW_5V_EN4` provides the 5 V input to the 12 V switch regulator, so it is
    reserved from public command handling in this MVP protocol
- `PDU_OUTPUT_BURN1`
  - separate deployment burn-wire channel 1
  - depends on `BURN1_EN` and shared `BURN_5V`
  - not latchable through `SET_OUTPUT_STATE`; use `FIRE_BURN_WIRE`
- `PDU_OUTPUT_BURN2`
  - separate deployment burn-wire channel 2
  - depends on `BURN2_EN` and shared `BURN_5V`
  - not latchable through `SET_OUTPUT_STATE`; use `FIRE_BURN_WIRE`

Coarse H-bridge group output IDs are reserved from generic `GET_OUTPUT_STATE` and
`SET_OUTPUT_STATE`. Torque control/readback only uses `SET_TORQUE_COIL` and
`GET_TORQUE_COIL`.

This keeps controller-side code focused on mission-level output intent instead
of board-pin choreography.

`GET_SUMMARY_STATUS` reports live H-bridge fault indications in its fault
bitmap. The current implementation treats the DRV8847-style fault lines as
active-low inputs.

`FAULT1` and `FAULT2` must stay configured as inputs in MCC. They are status
outputs from the DRV8847 H-bridge devices, not MCU control pins. Configuring
them as outputs would risk driving against the H-bridge fault output and could
mask or create invalid fault readings.

Torque-coil mapping:
- coil 1: `IN1/IN2`, U1 bridge 1/2
- coil 2: `IN3/IN4`, U1 bridge 3/4
- coil 3: `IN5/IN6`, U2 bridge 1/2
- coil 4: `IN7/IN8`, U2 bridge 3/4

Torque modes use the DRV8847 4-pin interface convention: `0/0=coast`,
`1/0=forward`, `0/1=reverse`, and `1/1=brake`. `TRQ1` is shared by coils 1 and
2, and `TRQ2` is shared by coils 3 and 4, so firmware rejects a current-scalar
change that would disturb an already-active sibling coil.

Timed torque commands are overrideable for the same coil so the OBC can stop or
replace an active pulse early. The firmware only tracks one timed torque pulse
at a time.

## Reset Info

`GET_RESET_INFO` reports the MCU reset cause register (`RSTC_RCAUSE`).

Useful bits:

- bit 0: power-on reset
- bit 1: core brownout reset
- bit 2: VDD brownout reset
- bit 4: external reset
- bit 5: watchdog reset
- bit 6: CPU system reset request

After `SOFTWARE_RESET`, the reported cause should be whichever hardware reset
source actually fired first, usually external reset or watchdog reset.

## Removed Legacy Code

The refactor removed these handwritten legacy paths from `src/app.c`:

- newline-delimited receive buffer
- text command parser (`read_CMD`)
- app-level `enableGPIOs()` / `disableGPIOs()` duplicates
- direct FatFS mount/write demo command
- unused I2C echo helper
- unused app state machine and SD file handles in `src/app.h`

Generated Harmony SD/FATFS configuration remains available for future work.
Generated SERCOM4 I2C slave configuration also remains available for future
work, but the handwritten app no longer initializes or uses it directly.

## Teensy Test Sketch Split

Keep operational test modes out of the PDU firmware. `ALL`, `VIBE`/vibration,
and `THERMAL` are implemented as separate Teensy-side sketches:

- `teensy/pdu_all_test/pdu_all_test.ino`
  - full bench checkout of UART link, protocol info, summary status, selected
    rail set/get, power-cycle behavior, torque-coil readback, and safe reset
    info
  - no automatic burn-wire firing with deployment hardware connected
- `teensy/pdu_vibration_test/pdu_vibration_test.ino`
  - autonomous vibration-test profile that commands outputs off, shuts charger
    down, coasts torque coils, and logs periodic PDU status to Teensy SD
- `teensy/pdu_thermal_test/pdu_thermal_test.ino`
  - autonomous thermal-vac profile that applies an editable rail/charger policy
    and logs PDU status plus TMP36/INA219 data to Teensy SD
- `teensy/pdu_test_common/pdu_test_common.h`
  - shared framed UART helper used by the profile sketches

The autonomous sketches also mirror logs to USB Serial when connected, but they
do not wait for a serial console at boot.
