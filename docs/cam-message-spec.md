# CAN Message Format Spec

**Version:** 1.1
**Last updated:** 2026-08-01
**Owner:** Person A (Central Node — CAN + FreeRTOS Lead)

## Purpose
A concise, consistent message contract between the central node and the sensor nodes (vibration, temperature). This is the single source of truth for CAN IDs, payload layout, and framing rules — all nodes must implement against this document exactly.

## Conventions
- **Identifier:** 11-bit standard CAN ID (CAN 2.0A / CAN FD base frame)
- **Framing:** v1 intentionally stays within classic CAN DLC limits (≤ 8 bytes) for simplicity, even though CAN FD supports up to 64-byte payloads. Larger payloads may be introduced in a future version if additional fields are needed.
- **Endianness:** Little-endian for multi-byte fields (LSB first). This matches the target architecture (Cortex-M, little-endian), so packed C structs can be used directly on-device. Do **not** reuse these structs unmodified on a big-endian host (e.g. when parsing logged data on a PC tool) without byte-swapping.
- **Signed integers:** Two's complement
- **Units and scale:** Documented per-field below. Unused bytes must be zero-padded.
- **Timestamps:** NOT included in the CAN payload. The central node stamps each message with a timestamp at time of reception (in the CAN RX task). Sensor nodes do not need to include or manage timestamps.

## Message Summary
| ID (hex) | Name | Source | DLC | Transmit Period |
|---:|---|---|:--:|---|
| 0x100 | VIBRATION_MEASUREMENT | Vibration node (C) | 8 | every 100 ms |
| 0x200 | TEMPERATURE_MEASUREMENT | Temperature node (D) | 4 | every 1000 ms |

*(Periods above are initial defaults — confirm/adjust with A once sensor sampling rates are finalized. Lower CAN ID = higher bus arbitration priority.)*

## Detailed Payloads

### VIBRATION_MEASUREMENT — ID 0x100 (DLC = 8)
**Purpose:** Periodic vibration sample (3-axis) with basic status.

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0–1 | Accel X | `i16` | scale = 0.01 g/LSB (signed) |
| 2–3 | Accel Y | `i16` | scale = 0.01 g/LSB (signed) |
| 4–5 | Accel Z | `i16` | scale = 0.01 g/LSB (signed) |
| 6 | Sample counter | `u8` | increments per sample, wraps 0–255 |
| 7 | Status flags | `u8` | bit0 = sensor error, bit1 = saturated, bits 2–7 reserved |

> Confirm the FXLS8974CF's configured measurement range (±2g/±4g/±8g/±16g) against this scale factor. At 0.01 g/LSB with `int16_t`, the field supports up to ±327 g theoretical range, so it comfortably covers any of the sensor's configured ranges — just make sure the configured range and expected real-world readings are consistent.

**Example (little-endian bytes):**
```
Payload: [0x2C, 0xFF, 0x10, 0x00, 0xE8, 0x03, 0x05, 0x00]
  Accel X = 0xFF2C -> -212       -> -2.12 g
  Accel Y = 0x0010 -> 16          -> 0.16 g
  Accel Z = 0x03E8 -> 1000        -> 10.00 g
  Sample counter = 5, status = 0
```

### TEMPERATURE_MEASUREMENT — ID 0x200 (DLC = 4)
**Purpose:** Periodic temperature telemetry.

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0–1 | Temperature | `i16` | scale = 0.01 °C/LSB (signed) |
| 2 | Status flags | `u8` | bit0 = fault, bit1 = busy, bits 2–7 reserved |
| 3 | Battery % | `u8` | 0–100, 255 = unknown |

**Example (25.34 °C, battery 100%):**
```
Temperature value = 25.34 °C -> 2534 decimal -> 0x09E6 -> bytes [0xE6, 0x09]
Payload: [0xE6, 0x09, 0x00, 0x64]
```

## Implementation Hint (packed C struct for vibration)
```c
struct __attribute__((packed)) vibration_msg {
    int16_t accel_x;        // bytes 0-1
    int16_t accel_y;        // bytes 2-3
    int16_t accel_z;        // bytes 4-5
    uint8_t sample_counter; // byte 6
    uint8_t status;         // byte 7
};
```

## Notes
- Lower CAN ID has higher bus arbitration priority.
- Keep DLC exact; zero-pad unused bytes.
- When changing any field's meaning, size, or scale, bump the document version and coordinate the change across all nodes before merging.
