#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

void LcdManager_UpdateIpv4Address(char ipv4[]);
void LcdManager_UpdateModbusDeviceAddress(uint8_t address);
void LcdManager_UpdateDigitalOutputStatus(uint8_t channel, bool value);
void LcdManager_UpdateDigitalInputStatus(uint8_t channel, bool value);

#endif  // LCD_MANAGER_H