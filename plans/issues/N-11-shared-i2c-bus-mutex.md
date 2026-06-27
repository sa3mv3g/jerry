# [N-11] Shared I2C bus used by LCD / Modbus-DO / EEPROM from different tasks without a BSP-level mutex

**Severity:** 🟡 Medium
**Component:** BSP I2C / LCD manager
**File:** `application/src/lcd_manager.c` (~281-309, 122-177); BSP I2C layer

## Description

`lcdManager_Send` → `BSP_I2C_LcdWrite` shares the I2C peripheral with `BSP_I2CDO_*` (digital output expander) and `BSP_EEPROM_*`, which are driven from other tasks (Modbus, DigitalOutput). There is no indication the I2C bus is mutex-protected at the BSP layer. `LcdManager_ShowVersionSplash()` also ignores `LcdI2c_WriteStringAt()` return values.

## Impact

Concurrent I2C transactions can interleave on the shared peripheral and corrupt transfers.

## Fix

Confirm the BSP I2C layer serializes access with a mutex; if not, add one.

> **Note (re N-01):** Making the Main task the single LCD writer resolves the shared-LCD-*state*
> race, but the shared **I2C peripheral** mutex is still required and is independent of that change.
