#include "lcd_manager.h"

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "bsp.h"
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
        /* LCD initialized successfully */
        LcdI2c_Clear(&lcd_handle);
        LcdI2c_WriteStringAt(&lcd_handle, 0, 0, "www.aics.co.in");
        LcdI2c_Clear(&lcd_handle);
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
    return BSP_I2C_Master_Write(i2c_address, (uint8_t*)data, length,
                                timeout_ms);
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
