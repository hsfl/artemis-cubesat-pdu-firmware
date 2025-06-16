# Artemis CubeSat – PDU MCU Firmware

## Project Metadata
- **Project Type**: Embedded Firmware
- **Target Platform**: SAME51 Microcontroller
- **Framework**: MPLAB Harmony v3
- **IDE**: MPLAB X
- **License**: MIT/Apache (see individual file headers)

## Overview

This repository contains the flight-software layer that runs on the SAME51 micro-controller of the **Power Distribution Unit (PDU)**.
It handles three core functionalities:

1. **System bring-up & main loop** (`main.c`) 
2. **Application logic** (initial-state, UART command handling, watchdog feed) (`app.c/.h`) 
3. **Binary command / telemetry packets** (`pdu_packet.c/.h`) 

## Dependencies
- MPLAB Harmony v3 Framework
- Microchip SAME51 BSP
- MAX16998 Watchdog Timer

## Technical Specifications

### Hardware Requirements
- SAME51 Microcontroller
- MAX16998 Watchdog Timer
- UART Interface
- GPIO Pins for Switch Control

### Software Architecture

#### Core Components
| Component | File | Description |
|-----------|------|-------------|
| System Initialization | `main.c` | Entry point and system initialization |
| Application Logic | `app.c/.h` | High-level behavior and state management |
| Protocol Layer | `pdu_packet.c/.h` | Command parsing and response generation |

### Communication Protocol

#### Supported Commands
| Op-code | Command | Description |
|---------|---------|-------------|
| `PING` | Ping | Returns `ACK`—used by the ground station for a liveness check. |
| `SET_SWITCH` | Set Switch | Turns any of the 3 V3/5 V/12 V rails, VBATT, burn-wires, or torque-coil H-bridge legs on or off. |
| `GET_SWITCH_STATUS` | Get Status | Reads back the current latch state of every switch for telemetry verification. |

*(All packet layouts and value enums live in `pdu_packet.h`; parsing logic is in `pdu_packet.c`.)*

### Project Structure
```
/artemis-cubesat-pdu-firmware
│  main.c              # System entry point
│  app.c              # Application logic
│  app.h              # Application interface
│  pdu_packet.c       # Protocol implementation
│  pdu_packet.h       # Protocol definitions
│  packs/             # Harmony-generated BSP
│  third_party/       # Third-party dependencies
└─ default/           # Harmony configuration
```

## Build & Deployment

### Prerequisites
- MPLAB X IDE
- XC32 Compiler
- Microchip Programmer

### Build Instructions
Please refer to the detailed build guide:
[Build & Flash Instructions](https://docs.google.com/document/d/1mCISQ2FT9NdC7M1yRxyKPuSgM1M17ViQa_bvZ7Jp-Uk/edit?usp=sharing)

## Development Guidelines

### Code Organization
- Each file has a single responsibility
- Hardware control is isolated in `pdu_packet.c`
- Application logic is contained in `app.c`
- System initialization in `main.c`

### Testing
- UART-based command interface for testing
- Watchdog timer monitoring
- Switch state verification

## License
See the top-of-file headers in each source for MIT/Apache notices as applicable.
