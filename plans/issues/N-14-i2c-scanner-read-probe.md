# [N-14] I2C scanner uses a read probe — can miss write-only devices and glitch PCF8574 outputs

**Severity:** 🟡 Medium
**Component:** I2C scanner
**File:** `application/src/i2c_scanner.c` (~96-104)

## Description

The scanner probes each address with a 1-byte **read** (`BSP_I2C_LcdRead`). Many devices are detected more reliably with a zero-length **write** / address-ACK probe. A read probe can (a) miss devices that NACK reads but ACK their address, and (b) for the PCF8574 output expander, momentarily drive the quasi-bidirectional pins, affecting outputs.

## Impact

Inaccurate scan results; possible glitch on PCF8574 outputs during a scan.

## Fix

Use an address-only / zero-length write probe (ACK detection) for discovery.
