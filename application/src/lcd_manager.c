#include "lcd_manager.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "app_version.h"
#include "bsp.h"
#include "bsp_i2c.h"
#include "lcd_i2c.h"
#include "semphr.h"
#include "task.h"

#define LCD_AI_FIELD_WIDTH 9
#define LCD_AI_BUFFER_SIZE (LCD_AI_FIELD_WIDTH + 1)
#define LCD_AI_FMT_STR \
    "%-" XSTR(LCD_AI_FIELD_WIDTH) "." XSTR(LCD_AI_FIELD_WIDTH) "s"
#define LCD_MANAGER_COLS_MAX                  (20U)
#define LCD_MANAGER_ROWS_MAX                  (4U)
#define LCD_MANAGER_ADDRESS_7BIT              (0x27U)
#define LCD_MANAGER_ADDRESS_8BIT              (LCD_MANAGER_ADDRESS_7BIT << 1U)
#define LCD_MANAGER_IPV4_ADDR_ROW_ID          (0U)
#define LCD_MANAGER_MODBUS_DEVICE_ADDR_ROW_ID (1U)
#define LCD_MANAGER_DIGITAL_OUTPUT_ROW_ID     (2U)
#define LCD_MANAGER_DIGITAL_INPUT_ROW_ID      (1U)

static char gLcdStrings[LCD_MANAGER_ROWS_MAX][LCD_MANAGER_COLS_MAX + 1];
static SemaphoreHandle_t gUpdateLcdSem;
static StaticSemaphore_t gUpdateLcdSemBuff;
static uint8_t           gIpLastOctet                    = 0;
static uint8_t           gModbusAddress                  = 0;
static uint8_t           gDigitalInput                   = 0;
static uint8_t           gRtcHours                       = 0;
static uint8_t           gRtcMinutes                     = 0;
static uint8_t           gRtcSeconds                     = 0;
static uint16_t          gDigitalOutput                  = 0;
static uint16_t          gAnalogOutput[4]                = {0};
static float             gAnalogInput[4]                 = {0.0f};
static volatile bool     gRowDirty[LCD_MANAGER_ROWS_MAX] = {false};
static LcdI2cHandle      lcd_handle;

/**
 * @brief Mark an LCD row as needing a redraw and wake the LCD task.
 * @param row Row index (0 .. LCD_MANAGER_ROWS_MAX-1).
 */
static void lcdManager_MarkRowDirty(uint8_t row)
{
    if (row < LCD_MANAGER_ROWS_MAX)
    {
        taskENTER_CRITICAL();
        gRowDirty[row] = true;
        taskEXIT_CRITICAL();

        if (gUpdateLcdSem != NULL)
        {
            (void)xSemaphoreGive(gUpdateLcdSem);
        }
    }
}

/**
 * @brief Atomically test-and-clear a row's dirty flag.
 * @param row Row index.
 * @return true if the row was dirty (and has now been cleared).
 */
static bool lcdManager_ClearRowDirty(uint8_t row)
{
    bool was_dirty = false;
    taskENTER_CRITICAL();
    if (gRowDirty[row])
    {
        gRowDirty[row] = false;
        was_dirty      = true;
    }
    taskEXIT_CRITICAL();
    return was_dirty;
}

/* Forward declaration */
static int32_t lcdManager_Send(uint8_t i2c_address, const uint8_t* data,
                               uint16_t length, uint32_t timeout_ms);

static void update_row_0(void)
{
    char temp[32];
    if (gRtcHours >= 24 || gRtcMinutes >= 60 || gRtcSeconds >= 60)
    {
        snprintf(temp, sizeof(temp), "IP:%02X MB:%02X --:--:--", gIpLastOctet,
                 gModbusAddress);
    }
    else
    {
        snprintf(temp, sizeof(temp), "IP:%02X MB:%02X %02u:%02u:%02u",
                 gIpLastOctet, gModbusAddress, (unsigned)gRtcHours,
                 (unsigned)gRtcMinutes, (unsigned)gRtcSeconds);
    }
    strncpy(gLcdStrings[0], temp, LCD_MANAGER_COLS_MAX);
    gLcdStrings[0][LCD_MANAGER_COLS_MAX] = '\0';
    lcdManager_MarkRowDirty(0U);
}

static void update_row_1(void)
{
    char temp[32];
    snprintf(temp, sizeof(temp), "%02X %04X %05u %05u ", gDigitalInput,
             gDigitalOutput, gAnalogOutput[0] % 100000U,
             gAnalogOutput[1] % 100000U);
    strncpy(gLcdStrings[1], temp, LCD_MANAGER_COLS_MAX);
    gLcdStrings[1][LCD_MANAGER_COLS_MAX] = '\0';
    lcdManager_MarkRowDirty(1U);
}

static void format_ai(char* buf, float val)
{
    if (isnan(val))
    {
        snprintf(buf, LCD_AI_BUFFER_SIZE, LCD_AI_FMT_STR, "NaN");
        return;
    }
    if (isinf(val))
    {
        snprintf(buf, LCD_AI_BUFFER_SIZE, LCD_AI_FMT_STR,
                 (val > 0) ? "+Inf" : "-Inf");
        return;
    }

    /* Clamp to a range that can be displayed in LCD_AI_FIELD_WIDTH chars with
     * sign and . */
    if (val > 99999.999f)
    {
        val = 99999.999f;
    }
    else if (val < -9999.999f)
    {
        val = -9999.999f;
    }

    char temp[16];
    int  len;

    if (val >= 10000.0f || val <= -1000.0f)
    {
        len = snprintf(temp, sizeof(temp), "%.0f", val);
    }
    else if (val >= 1000.0f || val <= -100.0f)
    {
        len = snprintf(temp, sizeof(temp), "%.1f", val);
    }
    else if (val >= 100.0f || val <= -10.0f)
    {
        len = snprintf(temp, sizeof(temp), "%.2f", val);
    }
    else
    {
        len = snprintf(temp, sizeof(temp), "%.3f", val);
    }

    /* Final check to prevent overflow, though clamping should prevent this. */
    if (len >= LCD_AI_BUFFER_SIZE)
    {
        snprintf(buf, LCD_AI_BUFFER_SIZE, LCD_AI_FMT_STR, "OVF");
    }
    else
    {
        snprintf(buf, LCD_AI_BUFFER_SIZE, LCD_AI_FMT_STR, temp);
    }
}

static void update_row_2(void)
{
    char ai0[10], ai1[10];
    format_ai(ai0, gAnalogInput[0]);
    format_ai(ai1, gAnalogInput[1]);
    snprintf(gLcdStrings[2], LCD_MANAGER_COLS_MAX + 1, "%s %s ", ai0, ai1);
    lcdManager_MarkRowDirty(2U);
}

static void update_row_3(void)
{
    char ai2[10], ai3[10];
    format_ai(ai2, gAnalogInput[2]);
    format_ai(ai3, gAnalogInput[3]);
    snprintf(gLcdStrings[3], LCD_MANAGER_COLS_MAX + 1, "%s %s ", ai2, ai3);
    lcdManager_MarkRowDirty(3U);
}

void vLcdManageTask(void* pvParameters)
{
    (void)pvParameters;
    LcdI2cError err;

    gUpdateLcdSem = xSemaphoreCreateBinaryStatic(&gUpdateLcdSemBuff);
    memset(gLcdStrings, (char)' ', sizeof(gLcdStrings));
    for (uint8_t lcdRow = 0; lcdRow < LCD_MANAGER_ROWS_MAX; lcdRow++)
    {
        gLcdStrings[lcdRow][LCD_MANAGER_COLS_MAX] = 0;
    }

    update_row_0();
    update_row_1();
    update_row_2();
    update_row_3();

    /* Configure LCD */
    LcdI2cConfig config = {.i2c_address       = LCD_MANAGER_ADDRESS_8BIT,
                           .display_size      = kLcdSize20x4,
                           .i2c_transmit      = lcdManager_Send,
                           .delay_us          = BSP_Delay_Us,
                           .backlight_enabled = true};

    /* Initialize LCD with app-managed handle */
    err = LcdI2c_Init(&lcd_handle, &config);

    if (err == kLcdI2cOk)
    {
        /* LCD initialized successfully — show version splash for 2 seconds */
        LcdI2c_Clear(&lcd_handle);
        LcdI2c_WriteStringAt(&lcd_handle, 0, 3, "www.aics.co.in");
        LcdManager_ShowVersionSplash(APP_VERSION_MAJOR, APP_VERSION_MINOR,
                                     APP_VERSION_PATCH, APP_BUILD_NUMBER,
                                     APP_GIT_HASH, 2000U);
    }

    HAL_IWDG_Refresh(&hiwdg);

    xEventGroupSync(xSyncEventGroup, APPTASK_LCDMANAGE_TASK_EVENT_MASK,
                    APPTASK_ALL_TASK_EVENT_MASK, portMAX_DELAY);

    /* Main task loop */
    for (;;)
    {
        /* Block until at least one row has been marked dirty. */
        if ((xSemaphoreTake(gUpdateLcdSem, portMAX_DELAY) == pdTRUE) &&
            (err == kLcdI2cOk))
        {
            bool any_dirty;

            /* Render dirty rows, re-scanning until a full pass finds none.
             * A row marked dirty while we are writing is therefore not lost
             * (BUG-09). */
            do
            {
                any_dirty = false;
                for (uint8_t lcdRow = 0; lcdRow < LCD_MANAGER_ROWS_MAX;
                     lcdRow++)
                {
                    if (lcdManager_ClearRowDirty(lcdRow))
                    {
                        any_dirty = true;
                        err       = LcdI2c_WriteStringAt(&lcd_handle, 0, lcdRow,
                                                         gLcdStrings[lcdRow]);
                    }
                }
            } while (any_dirty && (err == kLcdI2cOk));
        }
    }
}

int32_t lcdManager_Send(uint8_t i2c_address, const uint8_t* data,
                        uint16_t length, uint32_t timeout_ms)
{
    return BSP_I2C_LcdWrite(i2c_address, (uint8_t*)data, length, timeout_ms);
}

void LcdManager_IsLcdReady(SemaphoreHandle_t* isLcdReadySem)
{
    if ((NULL != isLcdReadySem) && (NULL != gUpdateLcdSem))
    {
        xSemaphoreGive(*isLcdReadySem);
    }
}

/**
 * @brief Parses a string into an 8-bit unsigned integer.
 * @param s The string to parse.
 * @param value Pointer to store the parsed value.
 * @return true if parsing was successful, false otherwise.
 */
static bool parse_uint8(const char* s, uint8_t* value)
{
    if (s == NULL || *s == '\0')
    {
        return false;
    }
    uint16_t result = 0;
    while (*s != '\0')
    {
        if (*s < '0' || *s > '9')
        {
            return false; /* Not a digit */
        }
        result = (uint16_t)(result * 10) + (uint16_t)(*s - '0');
        if (result > 255)
        {
            return false; /* Overflow */
        }
        s++;
    }
    *value = (uint8_t)result;
    return true;
}

void LcdManager_UpdateIpv4Address(const char* ip)
{
    if (ip != NULL)
    {
        const char* octet_str = ip;
        const char* last_dot  = strrchr(ip, '.');
        if (last_dot != NULL)
        {
            octet_str = last_dot + 1;
        }

        if (!parse_uint8(octet_str, &gIpLastOctet))
        {
            gIpLastOctet = 0; /* Default on parse error */
        }
        update_row_0();
    }
}

void LcdManager_UpdateModbusDeviceAddress(uint8_t address)
{
    gModbusAddress = address;
    update_row_0();
}

void LcdManager_UpdateDigitalOutputStatus(uint8_t channel, bool value)
{
    if (channel < 16)
    {
        if (value)
        {
            gDigitalOutput |= (1U << channel);
        }
        else
        {
            gDigitalOutput &= ~(1U << channel);
        }
        update_row_1();
    }
}

void LcdManager_UpdateDigitalInputStatus(uint8_t channel, bool value)
{
    if (channel < 8)
    {
        if (value)
        {
            gDigitalInput |= (1U << channel);
        }
        else
        {
            gDigitalInput &= ~(1U << channel);
        }
        update_row_1();
    }
}

void LcdManager_UpdateTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    gRtcHours   = hours;
    gRtcMinutes = minutes;
    gRtcSeconds = seconds;
    update_row_0();
}

void LcdManager_UpdateAnalogInput(uint8_t channel, float value)
{
    if (channel < 4)
    {
        gAnalogInput[channel] = value;
        if (channel < 2)
        {
            update_row_2();
        }
        else
        {
            update_row_3();
        }
    }
}

void LcdManager_UpdateAnalogOutput(uint8_t channel, uint16_t value)
{
    if (channel < 4)
    {
        gAnalogOutput[channel] = value;
        update_row_1();
    }
}

void LcdManager_ShowVersionSplash(uint16_t version_major,
                                  uint16_t version_minor,
                                  uint16_t version_patch, uint32_t build_number,
                                  const char* git_hash, uint32_t display_ms)
{
    char line[LCD_MANAGER_COLS_MAX + 1];

    /* Row 0: "v<major>.<minor>.<patch>" */
    snprintf(line, sizeof(line), "v%u.%u.%u", (unsigned)version_major,
             (unsigned)version_minor, (unsigned)version_patch);
    LcdI2c_WriteStringAt(&lcd_handle, 0, 0, line);

    /* Row 1: build number (Unix timestamp, up to 10 digits) */
    snprintf(line, sizeof(line), "%lu", (unsigned long)build_number);
    LcdI2c_WriteStringAt(&lcd_handle, 0, 1, line);

    /* Row 2: git commit hash (e.g. "9e1c7e83" or "9e1c7e83+" for dirty builds)
     */
    if (git_hash != NULL)
    {
        LcdI2c_WriteStringAt(&lcd_handle, 0, 2, git_hash);
    }

    /* Hold the splash screen for the requested duration */
    vTaskDelay(pdMS_TO_TICKS(display_ms));

    /* Clear the display so normal operation can begin */
    LcdI2c_Clear(&lcd_handle);
}
