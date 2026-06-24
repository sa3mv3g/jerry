#include "lcd_manager.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "app_version.h"
#include "bsp.h"
#include "bsp_i2c.h"
#include "lcd_i2c.h"
#include "semphr.h"
#include "task.h"

#define LCD_MANAGER_COLS_MAX                  (20U)
#define LCD_MANAGER_ROWS_MAX                  (4U)
#define LCD_MANAGER_ADDRESS_7BIT              (0x27U)
#define LCD_MANAGER_ADDRESS_8BIT              (LCD_MANAGER_ADDRESS_7BIT << 1U)
#define LCD_MANAGER_IPV4_ADDR_ROW_ID          (0U)
#define LCD_MANAGER_MODBUS_DEVICE_ADDR_ROW_ID (1U)
#define LCD_MANAGER_DIGITAL_OUTPUT_ROW_ID     (2U)
#define LCD_MANAGER_DIGITAL_INPUT_ROW_ID      (1U)

/* LCD_MANAGER_COLS_MAX + 1 cuz we need to store null character */
static char gLcdStrings[LCD_MANAGER_ROWS_MAX][LCD_MANAGER_COLS_MAX + 1];
static SemaphoreHandle_t gUdpateLcdSem;
static StaticSemaphore_t gUdpateLcdSemBuff;

/* Forward declaration */
static int32_t lcdManager_Send(uint8_t i2c_address, const uint8_t* data,
                               uint16_t length, uint32_t timeout_ms);

/* Application-managed LCD handle (static allocation) */
static LcdI2cHandle lcd_handle;

void vLcdManageTask(void* pvParameters)
{
    (void)pvParameters;
    LcdI2cError err;

    gUdpateLcdSem = xSemaphoreCreateBinaryStatic(&gUdpateLcdSemBuff);
    memset(gLcdStrings, (char)' ', sizeof(gLcdStrings));
    for (uint8_t lcdRow = 0; lcdRow < LCD_MANAGER_ROWS_MAX; lcdRow++)
    {
        gLcdStrings[lcdRow][LCD_MANAGER_COLS_MAX] = 0;
    }

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
        if ((xSemaphoreTake(gUdpateLcdSem, portMAX_DELAY) == pdTRUE) &&
            (err == kLcdI2cOk))
        {
            for (uint8_t lcdRow = 0; lcdRow < LCD_MANAGER_ROWS_MAX; lcdRow++)
            {
                err = LcdI2c_WriteStringAt(&lcd_handle, 0, lcdRow,
                                           gLcdStrings[lcdRow]);
            }
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
    if ((NULL != isLcdReadySem) && (NULL != gUdpateLcdSem))
    {
        xSemaphoreGive(isLcdReadySem);
    }
}

void LcdManager_UpdateIpv4Address(char ip[])
{
    if (NULL != gUdpateLcdSem)
    {
        snprintf(gLcdStrings[LCD_MANAGER_IPV4_ADDR_ROW_ID],
                 LCD_MANAGER_COLS_MAX, "IPv4:%s", ip);
        xSemaphoreGive(gUdpateLcdSem);
    }
}

void LcdManager_UpdateModbusDeviceAddress(uint8_t address)
{
    if (NULL != gUdpateLcdSem)
    {
        char* row = gLcdStrings[LCD_MANAGER_MODBUS_DEVICE_ADDR_ROW_ID];

        /* Manual conversion to avoid null terminator in the middle of buffer */
        if (address >= 100)
        {
            row[0] = '0' + (address / 100);
            row[1] = '0' + ((address / 10) % 10);
            row[2] = '0' + (address % 10);
        }
        else if (address >= 10)
        {
            row[0] = '0' + (address / 10);
            row[1] = '0' + (address % 10);
            row[2] = ' '; /* Preserve space padding */
        }
        else
        {
            row[0] = '0' + address;
            row[1] = ' '; /* Preserve space padding */
            row[2] = ' '; /* Preserve space padding */
        }

        xSemaphoreGive(gUdpateLcdSem);
    }
}

void LcdManager_UpdateDigitalOutputStatus(uint8_t channel, bool value)
{
    if (NULL != gUdpateLcdSem)
    {
        if (channel < 16)
        {
            if (value)
            {
                gLcdStrings[LCD_MANAGER_DIGITAL_OUTPUT_ROW_ID][channel] = '1';
            }
            else
            {
                gLcdStrings[LCD_MANAGER_DIGITAL_OUTPUT_ROW_ID][channel] = '0';
            }
            xSemaphoreGive(gUdpateLcdSem);
        }
    }
}

void LcdManager_UpdateDigitalInputStatus(uint8_t channel, bool value)
{
    if (NULL != gUdpateLcdSem)
    {
        if (channel < 8)
        {
            if (value)
            {
                gLcdStrings[LCD_MANAGER_DIGITAL_INPUT_ROW_ID]
                           [LCD_MANAGER_COLS_MAX + channel - 8] = '1';
            }
            else
            {
                gLcdStrings[LCD_MANAGER_DIGITAL_INPUT_ROW_ID]
                           [LCD_MANAGER_COLS_MAX + channel - 8] = '0';
            }
            xSemaphoreGive(gUdpateLcdSem);
        }
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
