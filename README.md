# Cargo Box

Cargo Box is a CAN-based embedded monitoring project for a cargo or asset tracking system. The repository is organized around a central node that receives telemetry from sensor nodes over a CAN bus and logs the data in a shared format.

## Overview

This project is intended to support:

- a central controller node running on FreeRTOS
- one or more sensor nodes measuring vibration and temperature
- CAN communication between the control node and sensors
- a common log entry format for captured CAN messages

## Repository structure

- `central-node/` — central control node project files and related MCU/board code
- `sensor-node/` — sensor node implementations
- `common/` — shared definitions and utilities
- `docs/` — project documentation and message specification
- `LICENSE` — project license

## CAN message contract

The project uses a documented CAN message definition in [docs/cam-message-spec.md](docs/cam-message-spec.md). It defines:

- CAN IDs for each sensor message type
- payload layout for vibration and temperature messages
- little-endian field encoding
- sample timing and status conventions

The shared log structure is defined in [common/log_entry.h](common/log_entry.h):

- `can_id`
- payload bytes
- `dlc`
- timestamp

## Key message types

- `0x100` — VIBRATION_MEASUREMENT
- `0x200` — TEMPERATURE_MEASUREMENT

These are intended for periodic telemetry from the sensor nodes to the central node.



## Notes

- The CAN protocol is designed around classic CAN payload limits for simplicity.
- The project is currently structured around embedded C and FreeRTOS-based firmware development.
- The message format is the source of truth and should remain consistent across all nodes.

## License

This project is licensed under the terms in the [LICENSE](LICENSE) file.
