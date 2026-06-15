# PDU Hardware Reference

## Purpose

The Artemis PDU regulates, controls, monitors, and distributes power for the
CubeSat. It connects the battery board, solar panels, antenna board, OBC/Teensy,
deployment safety circuits, burn-wire channels, torque coils, and end-user
power outputs.

This document should be read as the repo's manual-aligned hardware context.
The temporary student PDF ICD is useful historical context, but it is not the
current firmware or test-plan source of truth.

The firmware should expose logical spacecraft behavior. It should not require
the controller side to know board-pin choreography for composite rails, burn
wire sequencing, or H-bridge pin pairs.

## Major Interfaces

### PC/104 Header

The PC/104 header carries shared power and communication lines between stacked
CubeSat boards. Signal names can differ by board, but the physical lines are
shared.

Relevant firmware-facing lines include:

- UART between the PDU MCU and OBC/Teensy
- I2C sensor bus lines for INA219 current/power sensors
- analog temperature sensor lines routed toward OBC/Teensy ADC inputs
- power rails distributed between boards

### Battery Board Power

PDU battery-board connectors:

- `J16`: `PY_VBATT`, `GND`
- `J17`: `BATT_CHRG`, `GND`

Battery board connector:

- `J9`: duplicated pack positive and negative pins

The battery board uses Samsung 18650 lithium-ion cells and protection circuitry.
The PDU receives battery power and sends battery-charge power back through the
charging path.

### Solar Panel Inputs

PDU solar input connectors:

- `J1` to `J4`: solar power input from the four solar panel boards
- `J5` and `J6`: additional end-user/accessibility solar connectors

Each solar panel board has a 4-pin power connector with duplicated `VCC` and
`GND` pins. The manual lists each panel maximum as approximately:

- open circuit stack voltage: `10.35 V`
- current: `110.4 mA`
- power: `1.142 W`

### End-User Power Connectors

Breakout connector:

- `J9`
- exposes `SW_3V3_1`, `GND`, `SW_5V_3`, `GND`

12 V connector:

- `J10`
- exposes `12V_SW1` and `GND`
- firmware treats this as logical output `PDU_OUTPUT_12V`

Important: the 12 V output is composite. It depends on the upstream 5 V switch
that feeds the 12 V regulator plus the 12 V regulator enable/output path.

### Programming And Debug

JTAG/SWD connector:

- `J27`
- exposes `BUS_3V3`, `GND`, `SWDIO`, `SWCLK`, and reset

The PDU MCU is programmed/debugged through SWD/JTAG-style lines.

### USB-C

The USB-C receptacle carries:

- `UART2_RXD`
- `UART2_TXD`
- `USB_D+`
- `USB_D-`
- `VBUS`
- CC pins and ground

Current firmware scope:

- the UART path is the relevant command/debug path
- native USB device/CDC is not implemented
- USB-C should be treated as hardware present but firmware-incomplete

### MicroSD

The MicroSD socket carries SPI-style SD lines:

- `SD_CS`
- `SD_MOSI`
- `SD_MISO`
- `SD_SCK`
- `BUS_3V3`
- `GND`

Current firmware scope:

- generated Harmony SD/FATFS support remains present
- handwritten app runtime does not currently mount, read, or write MicroSD

## Power Rails And Regulators

### VBatt Filter

The PDU filters raw battery voltage with a passive low-pass filter. The manual
describes the intended battery voltage range as roughly `6.0 V` to `7.2 V`
when the CubeSat is powered through the safety chain.

### Always-On Buses

The PDU has two main always-on switching regulators when the spacecraft is
powered:

- `BUS_3V3`
  - regulator: `LT8609AEMSE#PBF`
  - input: `VBATT`
  - output: `3.3 V`
- `BUS_5V`
  - regulator: `LT8609AEMSE#PBF`
  - input: `VBATT`
  - output: `5 V`

These buses feed downstream switches, sensors, and control circuits.

### Switchable Outputs

3.3 V switches:

- `SW_3V3_1`: breakout/end-user power
- `SW_3V3_2`: spare/end-user use

5 V switches:

- `SW_5V_1`: Raspberry Pi Zero power on OBC
- `SW_5V_2`: Kapton heater
- `SW_5V_3`: breakout/end-user power
- `SW_5V_4`: upstream power into the 12 V regulator

VBatt switch:

- `SW_VBATT`: spare/end-user battery-voltage switch

Firmware policy:

- `SW_5V_4` is reserved from independent public control
- logical `PDU_OUTPUT_12V` owns the `SW_5V_4` dependency
- generic output commands must not expose a one-command enable-all path

### 12 V Switch Regulator

The 12 V output is generated from the 5 V switch-regulator path:

1. `SW_5V_4` feeds the 12 V regulator input path
2. first stage converts 5 V to an intermediate higher voltage
3. second stage regulates to usable 12 V output

Firmware should treat 12 V as one logical output even though it is physically
implemented through multiple enables/dependencies.

## Burn-Wire Deployment

The PDU has two burn-wire deployment channels:

- `BURN1`
- `BURN2`

Both channels depend on a shared burn source and a per-channel enable. These
are deployment mechanisms, not normal latchable rails.

Firmware policy:

- burn channels are not controlled through generic `SET_OUTPUT_STATE`
- burn channels are controlled only through `FIRE_BURN_WIRE`
- burn firing must be time-bounded
- burn firing must require an explicit arm token or equivalent safety gate
- startup/all-off behavior must force burn channels off

## H-Bridges And Torque Coils

The PDU uses two `DRV8847` H-bridge devices to drive torque coils embedded in
the four solar panel boards.

Solar panel torque-coil facts from the manual:

- two torque coils per solar panel
- supply voltage range: `3.3 V` to `5 V`
- nominal supply: `5 V`
- coil resistance: about `35.8 ohms`
- approximate single-coil power: `0.7 W`

Firmware-facing coil mapping:

- coil 1: H-bridge U1, `IN1/IN2`
- coil 2: H-bridge U1, `IN3/IN4`
- coil 3: H-bridge U2, `IN5/IN6`
- coil 4: H-bridge U2, `IN7/IN8`

Torque command modes:

- `coast`: input pair `0/0`
- `forward`: input pair `1/0`
- `reverse`: input pair `0/1`
- `brake`: input pair `1/1`

Firmware policy:

- Teensy/OBC runs the high-level ADCS algorithm
- PDU only exposes validated low-level torque-coil command/readback
- H-bridge fault lines are inputs, not outputs
- live fault bits are reported through summary status

## Sensors

### Temperature Sensors

The CubeSat has TMP36 analog temperature sensors:

- OBC: `AIN0`
- PDU: `AIN1`
- battery board: `AIN2`
- solar panels: `AIN3` to `AIN6`

PDU firmware currently does not expose full analog temperature telemetry in the
v2 UART protocol. The Teensy sensor sketch can directly test the relevant
analog/I2C lines during bench checkout.

### INA219 Current/Power Sensors

The PDU has INA219 current/power sensors for:

- solar input 1: I2C address `0x40`
- solar input 2: I2C address `0x41`
- solar input 3: I2C address `0x42`
- solar input 4: I2C address `0x43`
- VBatt: I2C address `0x44`

These are useful for solar input and battery-bus current/power checks. Full
sensor telemetry over the PDU protocol is future work.

## Battery Charging

The PDU battery charger controller uses `LTC4012IUF#PBF` to charge the
batteries from solar input. The manual describes the controller as configured
for a 2-cell Li-ion/Li-polymer battery pack with an adjusted charge voltage of
`8.4 V`.

Firmware-relevant signals include:

- `SHDN`: charger shutdown control line from MCU
- `CHRG`: charger status line
- `BATT_CHRG`: battery charging output path

LTC4012 datasheet behavior:

- `SHDN` is an active-low shutdown input. Drive `SHDN` high to enable/allow the
  charger, and drive `SHDN` low to shut the charger down.
- `CHRG` is an active-low open-drain charge indicator. A digital low means the
  charge indicator is active. Because the pin can also use weak-pulldown/high-Z
  states, firmware should document any board-level pull-up behavior before
  treating one GPIO read as detailed charge-state telemetry.

Charger command/state abstraction is not currently implemented in the v2
protocol. Do not add charger opcodes until the actual `SHDN` and `CHRG` MCU
pins are confirmed in MPLAB/Harmony pin configuration and generated port macros.
The current `src/config/default/pin_configurations.csv` does not expose named
`SHDN` or `CHRG` GPIOs, so implementing this now would require a deliberate
MPLAB configuration update.

Planned MPLAB pin assignments after schematic/config confirmation:

- physical pin 36 / `PB14`: `SHDN`, GPIO output, initial latch low for safe
  default charger shutdown
- physical pin 70 / `PA20`: `CHRG`, GPIO input, pull-up only if the board does
  not already provide the required pull-up

## Safety Circuits

### Inhibit Switch

The inhibit switch chain prevents power from flowing while the CubeSat is
inside the launcher. It connects through deployment switches on the structure
rails.

### Insert Before Flight

The IBF circuit is a battery-power disconnect/reconnect mechanism for ground
handling, testing, and integration. The manual describes PDU v2.2 IBF behavior
as a simple safety switch path controlling whether battery power reaches the
spacecraft load.

Firmware should assume these safety circuits can remove power asynchronously.

## Current Firmware Gaps

Hardware exists, but these features are not fully exposed by current firmware:

- USB-C native USB/CDC behavior
- MicroSD logging/config/file handling
- full temperature telemetry command path
- INA219 telemetry command path
- charger control/status command path; blocked until `SHDN` and `CHRG` pins
  are confirmed in generated MPLAB/Harmony config
- latched fault history beyond live H-bridge fault bits
