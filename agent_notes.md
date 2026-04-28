# Agent Notes

## Purpose

This file captures the current mission context and protocol-design direction for PDU protocol refactor work. It is a working engineering note for agents and developers, not a polished product spec.

## Mission Context

- This is not a full production EPS interface effort yet.
- The current target is a shortened FlatSat FSR end-to-end demo for the Neutron 2 team.
- Artemis hardware is the prototype/demo platform.
- Neutron 2 is the target mission architecture.

## System Architecture

- OBC is split across two processors:
  - Raspberry Pi Zero W: higher-level F' flight/demo logic
  - Teensy 4.1: low-level hardware-facing integration
- The active F' repo currently models EPS/PDU as a real subsystem boundary.
- The software-side EPS service is still mostly a stub/shim.

## Demo Story

1. Boot into Base Mode.
2. Determine mock ground-contact timing using D2S2 inputs.
3. Downlink live SOH telemetry for display.
4. Send a command that schedules data collection after a short delay.
5. Run data collection using real or simulated payload data.
6. Downlink science data.
7. Review/analyze science data on the ground PC using `fprime-gds`.

## Demo Priorities

- Stable nominal bring-up
- Visible SOH / health telemetry
- Reliable command acknowledgement
- Credible spacecraft power/health story
- Enough EPS/PDU functionality to support the demo path

## Expected PDU Boundary

The PDU should act as the spacecraft power regulation, switching, and safety/supervisory interface.

The Teensy should remain the low-level owner of:
- PDU UART/control path
- PDU current/power sensing over I2C
- analog temperature channels
- Pi enable/reset supervision

The Raspberry Pi / F' side should consume a simpler mission-facing EPS adapter interface, not raw pin-level PDU control.

## Current Firmware Reality

- Active packet handling is in `src/pdu_packet.c`.
- Active receive path is `src/app.c` -> `USART_READ()` -> `pdu_protocol_process_byte()`.
- The active protocol is now the framed binary protocol defined in `src/pdu_protocol_v2.h`.
- The ICD for the current protocol lives in `PDU_PROTOCOL_ICD.md`.
- The old ASCII-offset protocol is no longer the active runtime interface in this checkout.

## Current Protocol Direction

- Versioned framed binary transport
- Fixed-width integer fields only
- Explicit payload length
- CRC-16/CCITT integrity checking
- Sequence-numbered request/response behavior
- Mission-facing logical output IDs
- Summary status designed for SOH visibility

## Refactor Goal

Design and maintain a small, stable, binary supervision protocol between Teensy and PDU MCU that is:
- demo-first
- robust on UART
- explicit about acknowledgements and errors
- narrow in scope
- compatible with keeping safety decisions local to the PDU MCU

Do not optimize first for full mission completeness.

## What The Protocol Should Prioritize

- switched rail control/state
- charger state/control
- heater state/control
- Pi power/reset supervision
- PDU-local faults
- reset cause / watchdog / brownout information
- compact summary status for SOH

## What Should Stay Local To The PDU

- rail sequencing rules
- heater safety limits
- fault latching/inhibits
- watchdog behavior
- brownout handling
- any unsafe direct GPIO choreography

The external controller should request intent, not micromanage pins.

## Current Near-Term Scope

The current framed protocol implements:
- `GET_PROTOCOL_INFO`
- `GET_SUMMARY_STATUS`
- `GET_RESET_INFO`
- `GET_OUTPUT_STATE`
- `SET_OUTPUT_STATE`

That is enough to support:
- stable bring-up
- visible SOH
- reliable command acknowledgement
- direct output control for the demo path

## Practical Guidance For Follow-On Work

- Keep the Teensy<->PDU protocol small and stable.
- Let the Teensy adapt raw PDU details into a cleaner Pi/F' EPS-facing interface.
- Remove or hide transport-visible concepts that are really implementation details.
- Make the shared protocol header match actual implemented behavior at every step.
- Prefer idempotent commands such as "set state" over toggle-style commands.
- Treat `summary status` as a first-class product requirement because it supports the demo story directly.
