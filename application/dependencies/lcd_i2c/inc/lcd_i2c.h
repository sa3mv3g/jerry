/**
 * @file lcd_i2c.h
 * @brief LCD I2C Library for HD44780-compatible displays with PCF8574 I2C
 * expander
 *
 * This library provides an interface for controlling HD44780-compatible LCD
 * displays connected via PCF8574 I2C I/O expander. It supports 16x2 and 20x4
 * LCD configurations.
 *
 * Features:
 *   - No dynamic memory allocation (all static allocation)
 *   - Thread-safe design for FreeRTOS environments
 *   - Support for multiple LCD instances
 *   - Custom character support
 *   - Backlight control
 *
 * Hardware Connection (PCF8574 to LCD):
 *   P0 -> RS (Register Select)
 *   P1 -> RW (Read/Write) - typically tied to GND for write-only
 *   P2 -> EN (Enable)
 *   P3 -> Backlight control
 *   P4 -> D4
 *   P5 -> D5
 *   P6 -> D6
 *   P7 -> D7
 *
 * @copyright Copyright (c) 2026
 */

#ifndef LCD_I2C_H_
#define LCD_I2C_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup LCD_I2C_Config Configuration
 * @{
 */

/** Maximum number of custom characters (HD44780 supports 8) */
#define LCD_I2C_MAX_CUSTOM_CHARS (8U)

/** Custom character pattern size in bytes */
#define LCD_I2C_CUSTOM_CHAR_SIZE (8U)

    /** @} */

    /**
     * @defgroup LCD_I2C_Types Type Definitions
     * @{
     */

    /**
     * @brief LCD I2C error codes
     */
    typedef enum
    {
        kLcdI2cOk = 0,            /**< Operation successful */
        kLcdI2cErrorInvalidParam, /**< Invalid parameter provided */
        kLcdI2cErrorI2cFail,      /**< I2C communication failure */
        kLcdI2cErrorTimeout,      /**< Operation timed out */
        kLcdI2cErrorNotInit,      /**< LCD not initialized */
        kLcdI2cErrorBusy,         /**< LCD is busy */
        kLcdI2cErrorNoResource    /**< No available LCD instance */
    } LcdI2cError;

    /**
     * @brief LCD display size configuration
     */
    typedef enum
    {
        kLcdSize16x2 = 0, /**< 16 columns x 2 rows */
        kLcdSize20x4,     /**< 20 columns x 4 rows */
        kLcdSize16x4,     /**< 16 columns x 4 rows */
        kLcdSize20x2      /**< 20 columns x 2 rows */
    } LcdI2cSize;

    /**
     * @brief LCD cursor mode
     */
    typedef enum
    {
        kLcdCursorOff = 0, /**< Cursor hidden */
        kLcdCursorOn,      /**< Cursor visible (underline) */
        kLcdCursorBlink    /**< Cursor blinking block */
    } LcdI2cCursorMode;

    /**
     * @brief LCD text alignment for formatted output
     */
    typedef enum
    {
        kLcdAlignLeft = 0, /**< Left-aligned text */
        kLcdAlignCenter,   /**< Center-aligned text */
        kLcdAlignRight     /**< Right-aligned text */
    } LcdI2cAlignment;

    /**
     * @brief I2C transmit function pointer type
     *
     * User must provide this function to interface with their I2C HAL.
     *
     * @param i2c_address 7-bit I2C device address
     * @param data Pointer to data buffer to transmit
     * @param length Number of bytes to transmit
     * @param timeout_ms Timeout in milliseconds
     * @return 0 on success, non-zero on failure
     */
    typedef int32_t (*LcdI2cTransmitFunc)(uint8_t        i2c_address,
                                          const uint8_t *data, uint16_t length,
                                          uint32_t timeout_ms);

    /**
     * @brief Delay function pointer type
     *
     * User must provide this function for timing delays.
     *
     * @param delay_us Delay in microseconds
     */
    typedef void (*LcdI2cDelayFunc)(uint32_t delay_us);

    /**
     * @brief LCD I2C configuration structure
     */
    typedef struct
    {
        uint8_t            i2c_address;  /**< 7-bit I2C address of PCF8574 */
        LcdI2cSize         display_size; /**< LCD display size */
        LcdI2cTransmitFunc i2c_transmit; /**< I2C transmit function */
        LcdI2cDelayFunc    delay_us;     /**< Microsecond delay function */
        bool               backlight_enabled; /**< Initial backlight state */
    } LcdI2cConfig;

    /**
     * @brief LCD I2C handle structure
     *
     * This structure contains the internal state of an LCD instance.
     * Users allocate this structure and pass it to the library for
     * initialization.
     */
    typedef struct LcdI2cHandle
    {
        bool           initialized;      /**< Initialization flag */
        uint8_t        i2c_address;      /**< I2C address */
        uint8_t        cols;             /**< Number of columns */
        uint8_t        rows;             /**< Number of rows */
        uint8_t        backlight;        /**< Backlight state (0 or 0x08) */
        uint8_t        display_control;  /**< Display control register state */
        uint8_t        entry_mode;       /**< Entry mode register state */
        uint8_t        cursor_col;       /**< Current cursor column */
        uint8_t        cursor_row;       /**< Current cursor row */
        const uint8_t *row_offsets;      /**< Row offset table */
        LcdI2cTransmitFunc i2c_transmit; /**< I2C transmit function */
        LcdI2cDelayFunc    delay_us;     /**< Delay function */
    } LcdI2cHandle;

    /** @} */

    /**
     * @defgroup LCD_I2C_Init Initialization Functions
     * @{
     */

    /**
     * @brief Initialize an LCD I2C instance
     *
     * This function initializes the LCD display using the provided handle.
     * The LCD is configured in 4-bit mode. The user must allocate the handle
     * structure (statically, on stack, or dynamically) before calling this
     * function.
     *
     * @param[in,out] handle Pointer to LCD handle structure (user-allocated)
     * @param[in] config Pointer to configuration structure
     * @return LcdI2cError Error code
     *
     * @note The handle must remain valid for the lifetime of the LCD usage.
     * @note The configuration structure is copied internally, so it can be
     *       a temporary variable.
     *
     * Example:
     * @code
     * // Static allocation
     * static LcdI2cHandle lcd;
     *
     * LcdI2cConfig config = {
     *     .i2c_address = 0x27,
     *     .display_size = kLcdSize16x2,
     *     .i2c_transmit = my_i2c_transmit,
     *     .delay_us = my_delay_us,
     *     .backlight_enabled = true
     * };
     * LcdI2cError err = LcdI2c_Init(&lcd, &config);
     * @endcode
     */
    LcdI2cError LcdI2c_Init(LcdI2cHandle *handle, const LcdI2cConfig *config);

    /**
     * @brief Deinitialize an LCD I2C instance
     *
     * Releases the LCD instance and clears the display.
     *
     * @param[in] handle LCD handle
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_DeInit(LcdI2cHandle *handle);

    /** @} */

    /**
     * @defgroup LCD_I2C_Display Display Control Functions
     * @{
     */

    /**
     * @brief Clear the LCD display
     *
     * Clears all characters and returns cursor to home position.
     *
     * @param[in] handle LCD handle
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_Clear(LcdI2cHandle *handle);

    /**
     * @brief Return cursor to home position (0, 0)
     *
     * @param[in] handle LCD handle
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_Home(LcdI2cHandle *handle);

    /**
     * @brief Turn display on or off
     *
     * @param[in] handle LCD handle
     * @param[in] on true to turn on, false to turn off
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_DisplayOn(LcdI2cHandle *handle, bool on);

    /**
     * @brief Control the backlight
     *
     * @param[in] handle LCD handle
     * @param[in] on true to turn on, false to turn off
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_Backlight(LcdI2cHandle *handle, bool on);

    /**
     * @brief Set cursor display mode
     *
     * @param[in] handle LCD handle
     * @param[in] mode Cursor mode
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_SetCursor(LcdI2cHandle *handle, LcdI2cCursorMode mode);

    /** @} */

    /**
     * @defgroup LCD_I2C_Position Cursor Position Functions
     * @{
     */

    /**
     * @brief Set cursor position
     *
     * @param[in] handle LCD handle
     * @param[in] col Column position (0-indexed)
     * @param[in] row Row position (0-indexed)
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_SetPosition(LcdI2cHandle *handle, uint8_t col,
                                   uint8_t row);

    /**
     * @brief Get current cursor position
     *
     * @param[in] handle LCD handle
     * @param[out] col Pointer to store column position
     * @param[out] row Pointer to store row position
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_GetPosition(LcdI2cHandle *handle, uint8_t *col,
                                   uint8_t *row);

    /** @} */

    /**
     * @defgroup LCD_I2C_Write Write Functions
     * @{
     */

    /**
     * @brief Write a single character at current cursor position
     *
     * @param[in] handle LCD handle
     * @param[in] ch Character to write
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_WriteChar(LcdI2cHandle *handle, char ch);

    /**
     * @brief Write a null-terminated string at current cursor position
     *
     * @param[in] handle LCD handle
     * @param[in] str Null-terminated string to write
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_WriteString(LcdI2cHandle *handle, const char *str);

    /**
     * @brief Write a string at specified position
     *
     * @param[in] handle LCD handle
     * @param[in] col Column position (0-indexed)
     * @param[in] row Row position (0-indexed)
     * @param[in] str Null-terminated string to write
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_WriteStringAt(LcdI2cHandle *handle, uint8_t col,
                                     uint8_t row, const char *str);

    /**
     * @brief Write a string with alignment on specified row
     *
     * @param[in] handle LCD handle
     * @param[in] row Row position (0-indexed)
     * @param[in] str Null-terminated string to write
     * @param[in] alignment Text alignment
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_WriteAligned(LcdI2cHandle *handle, uint8_t row,
                                    const char *str, LcdI2cAlignment alignment);

    /**
     * @brief Write formatted output (printf-style)
     *
     * @param[in] handle LCD handle
     * @param[in] format Printf-style format string
     * @param[in] ... Variable arguments
     * @return LcdI2cError Error code
     *
     * @note Maximum output length is limited to display width.
     */
    LcdI2cError LcdI2c_Printf(LcdI2cHandle *handle, const char *format, ...)
        __attribute__((format(printf, 2, 3)));

    /**
     * @brief Write formatted output at specified position
     *
     * @param[in] handle LCD handle
     * @param[in] col Column position (0-indexed)
     * @param[in] row Row position (0-indexed)
     * @param[in] format Printf-style format string
     * @param[in] ... Variable arguments
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_PrintfAt(LcdI2cHandle *handle, uint8_t col, uint8_t row,
                                const char *format, ...)
        __attribute__((format(printf, 4, 5)));

    /** @} */

    /**
     * @defgroup LCD_I2C_Custom Custom Character Functions
     * @{
     */

    /**
     * @brief Create a custom character
     *
     * HD44780 supports up to 8 custom characters (locations 0-7).
     * Each character is 5x8 pixels, defined by 8 bytes.
     *
     * @param[in] handle LCD handle
     * @param[in] location Character location (0-7)
     * @param[in] pattern 8-byte pattern array (each byte defines one row)
     * @return LcdI2cError Error code
     *
     * Example:
     * @code
     * // Define a heart symbol
     * const uint8_t heart[8] = {
     *     0b00000,
     *     0b01010,
     *     0b11111,
     *     0b11111,
     *     0b01110,
     *     0b00100,
     *     0b00000,
     *     0b00000
     * };
     * LcdI2c_CreateChar(lcd, 0, heart);
     * LcdI2c_WriteChar(lcd, 0);  // Display the heart
     * @endcode
     */
    LcdI2cError LcdI2c_CreateChar(
        LcdI2cHandle *handle, uint8_t location,
        const uint8_t pattern[LCD_I2C_CUSTOM_CHAR_SIZE]);

    /** @} */

    /**
     * @defgroup LCD_I2C_Scroll Scroll Functions
     * @{
     */

    /**
     * @brief Scroll display content left
     *
     * @param[in] handle LCD handle
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_ScrollLeft(LcdI2cHandle *handle);

    /**
     * @brief Scroll display content right
     *
     * @param[in] handle LCD handle
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_ScrollRight(LcdI2cHandle *handle);

    /** @} */

    /**
     * @defgroup LCD_I2C_Utility Utility Functions
     * @{
     */

    /**
     * @brief Get display dimensions
     *
     * @param[in] handle LCD handle
     * @param[out] cols Pointer to store number of columns
     * @param[out] rows Pointer to store number of rows
     * @return LcdI2cError Error code
     */
    LcdI2cError LcdI2c_GetSize(LcdI2cHandle *handle, uint8_t *cols,
                               uint8_t *rows);

    /**
     * @brief Check if LCD is initialized
     *
     * @param[in] handle LCD handle
     * @return true if initialized, false otherwise
     */
    bool LcdI2c_IsInitialized(const LcdI2cHandle *handle);

    /**
     * @brief Get error string for error code
     *
     * @param[in] error Error code
     * @return Pointer to error string (static, do not free)
     */
    const char *LcdI2c_ErrorString(LcdI2cError error);

    /** @} */

#ifdef __cplusplus
}
#endif

#endif /* LCD_I2C_H_ */
