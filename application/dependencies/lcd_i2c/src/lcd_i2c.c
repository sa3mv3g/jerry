/**
 * @file lcd_i2c.c
 * @brief LCD I2C Library implementation for HD44780-compatible displays
 *
 * This file implements the LCD I2C library for controlling HD44780-compatible
 * LCD displays via PCF8574 I2C I/O expander.
 *
 * @copyright Copyright (c) 2026
 */

#include "lcd_i2c.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * @defgroup LCD_I2C_Private_Defines Private Defines
 * @{
 */

/* HD44780 Commands */
#define LCD_CMD_CLEAR_DISPLAY   (0x01U)
#define LCD_CMD_RETURN_HOME     (0x02U)
#define LCD_CMD_ENTRY_MODE_SET  (0x04U)
#define LCD_CMD_DISPLAY_CONTROL (0x08U)
#define LCD_CMD_CURSOR_SHIFT    (0x10U)
#define LCD_CMD_FUNCTION_SET    (0x20U)
#define LCD_CMD_SET_CGRAM_ADDR  (0x40U)
#define LCD_CMD_SET_DDRAM_ADDR  (0x80U)

/* Entry Mode Set flags */
#define LCD_ENTRY_RIGHT     (0x00U)
#define LCD_ENTRY_LEFT      (0x02U)
#define LCD_ENTRY_SHIFT_INC (0x01U)
#define LCD_ENTRY_SHIFT_DEC (0x00U)

/* Display Control flags */
#define LCD_DISPLAY_ON  (0x04U)
#define LCD_DISPLAY_OFF (0x00U)
#define LCD_CURSOR_ON   (0x02U)
#define LCD_CURSOR_OFF  (0x00U)
#define LCD_BLINK_ON    (0x01U)
#define LCD_BLINK_OFF   (0x00U)

/* Cursor/Display Shift flags */
#define LCD_DISPLAY_MOVE (0x08U)
#define LCD_CURSOR_MOVE  (0x00U)
#define LCD_MOVE_RIGHT   (0x04U)
#define LCD_MOVE_LEFT    (0x00U)

/* Function Set flags */
#define LCD_8BIT_MODE (0x10U)
#define LCD_4BIT_MODE (0x00U)
#define LCD_2LINE     (0x08U)
#define LCD_1LINE     (0x00U)
#define LCD_5x10_DOTS (0x04U)
#define LCD_5x8_DOTS  (0x00U)

/* PCF8574 pin mapping */
#define LCD_PIN_RS        (0x01U) /* P0 - Register Select */
#define LCD_PIN_RW        (0x02U) /* P1 - Read/Write */
#define LCD_PIN_EN        (0x04U) /* P2 - Enable */
#define LCD_PIN_BACKLIGHT (0x08U) /* P3 - Backlight */
#define LCD_PIN_D4        (0x10U) /* P4 - Data bit 4 */
#define LCD_PIN_D5        (0x20U) /* P5 - Data bit 5 */
#define LCD_PIN_D6        (0x40U) /* P6 - Data bit 6 */
#define LCD_PIN_D7        (0x80U) /* P7 - Data bit 7 */

/* Timing constants (microseconds) */
#define LCD_DELAY_POWER_ON_US (50000U) /* >40ms after power on */
#define LCD_DELAY_INIT_1_US   (4500U)  /* >4.1ms */
#define LCD_DELAY_INIT_2_US   (150U)   /* >100us */
#define LCD_DELAY_ENABLE_US   (2U)     /* Enable pulse width */
#define LCD_DELAY_COMMAND_US  (50U)    /* Command execution time */
#define LCD_DELAY_CLEAR_US    (2000U)  /* Clear/Home command time */

/* Row offsets for different LCD sizes */
#define LCD_ROW_OFFSET_16x2_0 (0x00U)
#define LCD_ROW_OFFSET_16x2_1 (0x40U)
#define LCD_ROW_OFFSET_20x4_0 (0x00U)
#define LCD_ROW_OFFSET_20x4_1 (0x40U)
#define LCD_ROW_OFFSET_20x4_2 (0x14U)
#define LCD_ROW_OFFSET_20x4_3 (0x54U)

/* I2C timeout */
#define LCD_I2C_TIMEOUT_MS (100U)

/* Printf buffer size */
#define LCD_PRINTF_BUFFER_SIZE (41U) /* Max 40 chars + null terminator */

/** @} */

/** @} */

/**
 * @defgroup LCD_I2C_Private_Variables Private Variables
 * @{
 */

/** Row offsets for 16x2 LCD */
static const uint8_t row_offsets_16x2[2] = {LCD_ROW_OFFSET_16x2_0,
                                            LCD_ROW_OFFSET_16x2_1};

/** Row offsets for 20x4 LCD */
static const uint8_t row_offsets_20x4[4] = {
    LCD_ROW_OFFSET_20x4_0, LCD_ROW_OFFSET_20x4_1, LCD_ROW_OFFSET_20x4_2,
    LCD_ROW_OFFSET_20x4_3};

/** Row offsets for 16x4 LCD */
static const uint8_t row_offsets_16x4[4] = {0x00U, 0x40U, 0x10U, 0x50U};

/** Row offsets for 20x2 LCD */
static const uint8_t row_offsets_20x2[2] = {0x00U, 0x40U};

/** Error strings */
static const char *const error_strings[] = {"OK",
                                            "Invalid parameter",
                                            "I2C communication failure",
                                            "Operation timed out",
                                            "LCD not initialized",
                                            "LCD is busy",
                                            "No available LCD instance"};

/** @} */

/**
 * @defgroup LCD_I2C_Private_Functions Private Functions
 * @{
 */

/**
 * @brief Write a byte to the PCF8574
 * @param handle LCD handle
 * @param data Byte to write
 * @return LcdI2cError Error code
 */
static LcdI2cError WriteExpander(LcdI2cHandle *handle, uint8_t data)
{
    uint8_t tx_data = data | handle->backlight;
    int32_t result  = handle->i2c_transmit(handle->i2c_address, &tx_data, 1U,
                                           LCD_I2C_TIMEOUT_MS);
    return (result == 0) ? kLcdI2cOk : kLcdI2cErrorI2cFail;
}

/**
 * @brief Pulse the enable pin
 * @param handle LCD handle
 * @param data Data byte with EN bit to pulse
 * @return LcdI2cError Error code
 */
static LcdI2cError PulseEnable(LcdI2cHandle *handle, uint8_t data)
{
    LcdI2cError err;

    /* Set EN high */
    err = WriteExpander(handle, data | LCD_PIN_EN);
    if (err != kLcdI2cOk)
    {
        return err;
    }
    handle->delay_us(LCD_DELAY_ENABLE_US);

    /* Set EN low */
    err = WriteExpander(handle, data & (uint8_t)(~LCD_PIN_EN));
    if (err != kLcdI2cOk)
    {
        return err;
    }
    handle->delay_us(LCD_DELAY_COMMAND_US);

    return kLcdI2cOk;
}

/**
 * @brief Write 4 bits to the LCD
 * @param handle LCD handle
 * @param nibble 4-bit value (in upper nibble position)
 * @return LcdI2cError Error code
 */
static LcdI2cError Write4Bits(LcdI2cHandle *handle, uint8_t nibble)
{
    LcdI2cError err;

    err = WriteExpander(handle, nibble);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    return PulseEnable(handle, nibble);
}

/**
 * @brief Send a byte to the LCD (command or data)
 * @param handle LCD handle
 * @param value Byte to send
 * @param mode 0 for command, LCD_PIN_RS for data
 * @return LcdI2cError Error code
 */
static LcdI2cError SendByte(LcdI2cHandle *handle, uint8_t value, uint8_t mode)
{
    LcdI2cError err;
    uint8_t     high_nibble = (value & 0xF0U) | mode;
    uint8_t     low_nibble  = ((value << 4U) & 0xF0U) | mode;

    err = Write4Bits(handle, high_nibble);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    return Write4Bits(handle, low_nibble);
}

/**
 * @brief Send a command to the LCD
 * @param handle LCD handle
 * @param cmd Command byte
 * @return LcdI2cError Error code
 */
static LcdI2cError SendCommand(LcdI2cHandle *handle, uint8_t cmd)
{
    return SendByte(handle, cmd, 0U);
}

/**
 * @brief Send data to the LCD
 * @param handle LCD handle
 * @param data Data byte
 * @return LcdI2cError Error code
 */
static LcdI2cError SendData(LcdI2cHandle *handle, uint8_t data)
{
    return SendByte(handle, data, LCD_PIN_RS);
}

/**
 * @brief Get display dimensions from size enum
 * @param size Display size enum
 * @param cols Pointer to store columns
 * @param rows Pointer to store rows
 * @param offsets Pointer to store row offsets table
 */
static void GetDisplayDimensions(LcdI2cSize size, uint8_t *cols, uint8_t *rows,
                                 const uint8_t **offsets)
{
    switch (size)
    {
        case kLcdSize16x2:
            *cols    = 16U;
            *rows    = 2U;
            *offsets = row_offsets_16x2;
            break;
        case kLcdSize20x4:
            *cols    = 20U;
            *rows    = 4U;
            *offsets = row_offsets_20x4;
            break;
        case kLcdSize16x4:
            *cols    = 16U;
            *rows    = 4U;
            *offsets = row_offsets_16x4;
            break;
        case kLcdSize20x2:
            *cols    = 20U;
            *rows    = 2U;
            *offsets = row_offsets_20x2;
            break;
        default:
            *cols    = 16U;
            *rows    = 2U;
            *offsets = row_offsets_16x2;
            break;
    }
}

/** @} */

/**
 * @defgroup LCD_I2C_Public_Functions Public Functions
 * @{
 */

LcdI2cError LcdI2c_Init(LcdI2cHandle *handle, const LcdI2cConfig *config)
{
    LcdI2cError err;

    /* Validate parameters */
    if ((handle == NULL) || (config == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    if ((config->i2c_transmit == NULL) || (config->delay_us == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    /* Clear the handle structure */
    (void)memset(handle, 0, sizeof(LcdI2cHandle));

    /* Initialize handle with configuration */
    handle->i2c_address  = config->i2c_address;
    handle->i2c_transmit = config->i2c_transmit;
    handle->delay_us     = config->delay_us;
    handle->backlight    = config->backlight_enabled ? LCD_PIN_BACKLIGHT : 0U;

    GetDisplayDimensions(config->display_size, &handle->cols, &handle->rows,
                         &handle->row_offsets);

    /* Wait for LCD power-on */
    handle->delay_us(LCD_DELAY_POWER_ON_US);

    /* Initialize in 4-bit mode per HD44780 datasheet */
    /* First, put LCD into 4-bit mode */
    /* We start in 8-bit mode, try to set 4-bit mode */
    err = Write4Bits(handle, 0x30U); /* Function set: 8-bit */
    if (err != kLcdI2cOk)
    {
        return err;
    }
    handle->delay_us(LCD_DELAY_INIT_1_US);

    err = Write4Bits(handle, 0x30U); /* Function set: 8-bit */
    if (err != kLcdI2cOk)
    {
        return err;
    }
    handle->delay_us(LCD_DELAY_INIT_2_US);

    err = Write4Bits(handle, 0x30U); /* Function set: 8-bit */
    if (err != kLcdI2cOk)
    {
        return err;
    }
    handle->delay_us(LCD_DELAY_INIT_2_US);

    /* Now set to 4-bit mode */
    err = Write4Bits(handle, 0x20U); /* Function set: 4-bit */
    if (err != kLcdI2cOk)
    {
        return err;
    }

    /* Configure display: 4-bit, 2-line, 5x8 dots */
    uint8_t function_set = LCD_CMD_FUNCTION_SET | LCD_4BIT_MODE | LCD_5x8_DOTS;
    if (handle->rows > 1U)
    {
        function_set |= LCD_2LINE;
    }
    err = SendCommand(handle, function_set);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    /* Display control: display on, cursor off, blink off */
    handle->display_control = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;
    err =
        SendCommand(handle, LCD_CMD_DISPLAY_CONTROL | handle->display_control);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    /* Clear display */
    err = SendCommand(handle, LCD_CMD_CLEAR_DISPLAY);
    if (err != kLcdI2cOk)
    {
        return err;
    }
    handle->delay_us(LCD_DELAY_CLEAR_US);

    /* Entry mode: increment, no shift */
    handle->entry_mode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DEC;
    err = SendCommand(handle, LCD_CMD_ENTRY_MODE_SET | handle->entry_mode);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    /* Initialize cursor position */
    handle->cursor_col = 0U;
    handle->cursor_row = 0U;

    handle->initialized = true;

    return kLcdI2cOk;
}

LcdI2cError LcdI2c_DeInit(LcdI2cHandle *handle)
{
    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    /* Clear display and turn off backlight */
    (void)SendCommand(handle, LCD_CMD_CLEAR_DISPLAY);
    handle->backlight = 0U;
    (void)WriteExpander(handle, 0U);

    /* Mark as uninitialized */
    handle->initialized = false;

    return kLcdI2cOk;
}

LcdI2cError LcdI2c_Clear(LcdI2cHandle *handle)
{
    LcdI2cError err;

    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    err = SendCommand(handle, LCD_CMD_CLEAR_DISPLAY);
    if (err == kLcdI2cOk)
    {
        handle->delay_us(LCD_DELAY_CLEAR_US);
        handle->cursor_col = 0U;
        handle->cursor_row = 0U;
    }

    return err;
}

LcdI2cError LcdI2c_Home(LcdI2cHandle *handle)
{
    LcdI2cError err;

    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    err = SendCommand(handle, LCD_CMD_RETURN_HOME);
    if (err == kLcdI2cOk)
    {
        handle->delay_us(LCD_DELAY_CLEAR_US);
        handle->cursor_col = 0U;
        handle->cursor_row = 0U;
    }

    return err;
}

LcdI2cError LcdI2c_DisplayOn(LcdI2cHandle *handle, bool on)
{
    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    if (on)
    {
        handle->display_control |= LCD_DISPLAY_ON;
    }
    else
    {
        handle->display_control &= (uint8_t)(~LCD_DISPLAY_ON);
    }

    return SendCommand(handle,
                       LCD_CMD_DISPLAY_CONTROL | handle->display_control);
}

LcdI2cError LcdI2c_Backlight(LcdI2cHandle *handle, bool on)
{
    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    handle->backlight = on ? LCD_PIN_BACKLIGHT : 0U;

    return WriteExpander(handle, handle->backlight);
}

LcdI2cError LcdI2c_SetCursor(LcdI2cHandle *handle, LcdI2cCursorMode mode)
{
    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    /* Clear cursor bits */
    handle->display_control &= (uint8_t)(~(LCD_CURSOR_ON | LCD_BLINK_ON));

    switch (mode)
    {
        case kLcdCursorOn:
            handle->display_control |= LCD_CURSOR_ON;
            break;
        case kLcdCursorBlink:
            handle->display_control |= (LCD_CURSOR_ON | LCD_BLINK_ON);
            break;
        case kLcdCursorOff:
        default:
            /* Cursor bits already cleared */
            break;
    }

    return SendCommand(handle,
                       LCD_CMD_DISPLAY_CONTROL | handle->display_control);
}

LcdI2cError LcdI2c_SetPosition(LcdI2cHandle *handle, uint8_t col, uint8_t row)
{
    LcdI2cError err;

    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    /* Clamp to valid range */
    if (col >= handle->cols)
    {
        col = handle->cols - (uint8_t)1U;
    }
    if (row >= handle->rows)
    {
        row = handle->rows - (uint8_t)1U;
    }

    uint8_t address = handle->row_offsets[row] + col;
    err             = SendCommand(handle, LCD_CMD_SET_DDRAM_ADDR | address);

    if (err == kLcdI2cOk)
    {
        handle->cursor_col = col;
        handle->cursor_row = row;
    }

    return err;
}

LcdI2cError LcdI2c_GetPosition(LcdI2cHandle *handle, uint8_t *col, uint8_t *row)
{
    if ((handle == NULL) || (col == NULL) || (row == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    *col = handle->cursor_col;
    *row = handle->cursor_row;

    return kLcdI2cOk;
}

LcdI2cError LcdI2c_WriteChar(LcdI2cHandle *handle, char ch)
{
    LcdI2cError err;

    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    err = SendData(handle, (uint8_t)ch);

    if (err == kLcdI2cOk)
    {
        handle->cursor_col++;
        if (handle->cursor_col >= handle->cols)
        {
            handle->cursor_col = 0U;
            handle->cursor_row++;
            if (handle->cursor_row >= handle->rows)
            {
                handle->cursor_row = 0U;
            }
        }
    }

    return err;
}

LcdI2cError LcdI2c_WriteString(LcdI2cHandle *handle, const char *str)
{
    LcdI2cError err = kLcdI2cOk;

    if ((handle == NULL) || (str == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    while ((*str != '\0') && (err == kLcdI2cOk))
    {
        err = LcdI2c_WriteChar(handle, *str);
        str++;
    }

    return err;
}

LcdI2cError LcdI2c_WriteStringAt(LcdI2cHandle *handle, uint8_t col, uint8_t row,
                                 const char *str)
{
    LcdI2cError err;

    err = LcdI2c_SetPosition(handle, col, row);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    return LcdI2c_WriteString(handle, str);
}

LcdI2cError LcdI2c_WriteAligned(LcdI2cHandle *handle, uint8_t row,
                                const char *str, LcdI2cAlignment alignment)
{
    uint8_t col = 0U;
    size_t  len;

    if ((handle == NULL) || (str == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    len = strlen(str);
    if (len > (size_t)handle->cols)
    {
        len = (size_t)handle->cols;
    }

    switch (alignment)
    {
        case kLcdAlignCenter:
            col = (uint8_t)((handle->cols - (uint8_t)len) / 2U);
            break;
        case kLcdAlignRight:
            col = (uint8_t)(handle->cols - (uint8_t)len);
            break;
        case kLcdAlignLeft:
        default:
            col = 0U;
            break;
    }

    return LcdI2c_WriteStringAt(handle, col, row, str);
}

LcdI2cError LcdI2c_Printf(LcdI2cHandle *handle, const char *format, ...)
{
    char    buffer[LCD_PRINTF_BUFFER_SIZE];
    va_list args;

    if ((handle == NULL) || (format == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return LcdI2c_WriteString(handle, buffer);
}

LcdI2cError LcdI2c_PrintfAt(LcdI2cHandle *handle, uint8_t col, uint8_t row,
                            const char *format, ...)
{
    char        buffer[LCD_PRINTF_BUFFER_SIZE];
    va_list     args;
    LcdI2cError err;

    if ((handle == NULL) || (format == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    err = LcdI2c_SetPosition(handle, col, row);
    if (err != kLcdI2cOk)
    {
        return err;
    }

    va_start(args, format);
    (void)vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return LcdI2c_WriteString(handle, buffer);
}

LcdI2cError LcdI2c_CreateChar(LcdI2cHandle *handle, uint8_t location,
                              const uint8_t pattern[LCD_I2C_CUSTOM_CHAR_SIZE])
{
    LcdI2cError err;

    if ((handle == NULL) || (pattern == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    if (location >= LCD_I2C_MAX_CUSTOM_CHARS)
    {
        return kLcdI2cErrorInvalidParam;
    }

    /* Set CGRAM address */
    err = SendCommand(handle, LCD_CMD_SET_CGRAM_ADDR | (location << 3U));
    if (err != kLcdI2cOk)
    {
        return err;
    }

    /* Write pattern data */
    for (uint8_t i = 0U; i < LCD_I2C_CUSTOM_CHAR_SIZE; i++)
    {
        err = SendData(handle, pattern[i]);
        if (err != kLcdI2cOk)
        {
            return err;
        }
    }

    /* Return to DDRAM */
    return LcdI2c_SetPosition(handle, handle->cursor_col, handle->cursor_row);
}

LcdI2cError LcdI2c_ScrollLeft(LcdI2cHandle *handle)
{
    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    return SendCommand(handle,
                       LCD_CMD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_LEFT);
}

LcdI2cError LcdI2c_ScrollRight(LcdI2cHandle *handle)
{
    if (handle == NULL)
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    return SendCommand(
        handle, LCD_CMD_CURSOR_SHIFT | LCD_DISPLAY_MOVE | LCD_MOVE_RIGHT);
}

LcdI2cError LcdI2c_GetSize(LcdI2cHandle *handle, uint8_t *cols, uint8_t *rows)
{
    if ((handle == NULL) || (cols == NULL) || (rows == NULL))
    {
        return kLcdI2cErrorInvalidParam;
    }

    if (!handle->initialized)
    {
        return kLcdI2cErrorNotInit;
    }

    *cols = handle->cols;
    *rows = handle->rows;

    return kLcdI2cOk;
}

bool LcdI2c_IsInitialized(const LcdI2cHandle *handle)
{
    if (handle == NULL)
    {
        return false;
    }
    return handle->initialized;
}

const char *LcdI2c_ErrorString(LcdI2cError error)
{
    if ((uint32_t)error >= (sizeof(error_strings) / sizeof(error_strings[0])))
    {
        return "Unknown error";
    }
    return error_strings[error];
}

/** @} */
