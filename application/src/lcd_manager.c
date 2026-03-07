#include "lcd_manager.h"

#include "FreeRTOS.h"
#include "app_tasks.h"
#include "bsp.h"
#include "lcd_i2c.h"
#include "semphr.h"
#include "task.h"

#define LCD_MANAGER_COLS_MAX         (20U)
#define LCD_MANAGER_ROWS_MAX         (4U)
#define LCD_MANAGER_ADDRESS_7BIT     (0x27U)
#define LCD_MANAGER_ADDRESS_8BIT     (LCD_MANAGER_ADDRESS_7BIT << 1U)
#define LCD_MANAGER_IPV4_ADDR_ROW_ID (0U)

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

    memset(gLcdStrings, 0, sizeof(gLcdStrings));

    /* Configure LCD */
    LcdI2cConfig config = {.i2c_address       = LCD_MANAGER_ADDRESS_8BIT,
                           .display_size      = kLcdSize20x4,
                           .i2c_transmit      = lcdManager_Send,
                           .delay_us          = BSP_Delay_Us,
                           .backlight_enabled = true};

    /* Initialize LCD with app-managed handle */
    err = LcdI2c_Init(&lcd_handle, &config);
    if (err != kLcdI2cOk)
    {
        /* Handle initialization error */
        /* Could log error or retry */
        for (;;)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /* LCD initialized successfully - display welcome message */
    LcdI2c_Clear(&lcd_handle);

    /* Main task loop */
    for (;;)
    {
        if (xSemaphoreTake(gUdpateLcdSem, portMAX_DELAY) == pdTRUE)
        {
            for (uint8_t lcdRow = 0; lcdRow < LCD_MANAGER_ROWS_MAX; lcdRow++)
            {
                LcdI2c_WriteStringAt(&lcd_handle, 0, lcdRow,
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

void LcdManager_UpdateIpv4Address(char ip[])
{
    snprintf(gLcdStrings[LCD_MANAGER_IPV4_ADDR_ROW_ID],
             LCD_MANAGER_COLS_MAX + 1, "IPv4:%s", ip);
    xSemaphoreGive(gUdpateLcdSem);
}
