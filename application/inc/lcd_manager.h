#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Display firmware version, build number, and git commit hash on the
 *        LCD as a startup splash screen.
 *
 * Writes version info directly to the LCD hardware (bypassing the row-buffer
 * semaphore mechanism) and holds the display for @p display_ms milliseconds.
 * After the delay the LCD is cleared so normal operation can begin.
 *
 * LCD layout (20×4):
 * @code
 *   Row 0: "v<major>.<minor>.<patch>"
 *   Row 1: "<build_number>"          (Unix timestamp)
 *   Row 2: "<git_hash>"              (e.g. "9e1c7e83" or "9e1c7e83+")
 * @endcode
 *
 * Intended to be called once from vLcdManageTask() immediately after the LCD
 * is successfully initialised, before the sync event group barrier.
 *
 * @param version_major  Application version major number.
 * @param version_minor  Application version minor number.
 * @param version_patch  Application version patch number.
 * @param build_number   Build number (Unix timestamp, uint32_t).
 * @param git_hash       Null-terminated git commit hash string (e.g.
 * "9e1c7e83").
 * @param display_ms     Duration in milliseconds to show the splash screen.
 */
void LcdManager_ShowVersionSplash(uint16_t version_major,
                                  uint16_t version_minor,
                                  uint16_t version_patch, uint32_t build_number,
                                  const char *git_hash, uint32_t display_ms);

void LcdManager_UpdateIpv4Address(char ipv4[]);
void LcdManager_UpdateModbusDeviceAddress(uint8_t address);
void LcdManager_UpdateDigitalOutputStatus(uint8_t channel, bool value);
void LcdManager_UpdateDigitalInputStatus(uint8_t channel, bool value);

#endif  // LCD_MANAGER_H