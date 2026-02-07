#include "bsp.h"

#include "main.h"
#include "stm32f4xx_hal.h"
#include "lwip.h"
#include "usb_host.h"

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_ADC1_Init(void);
void MX_ADC2_Init(void);
void MX_USART3_UART_Init(void);

bsp_error_t BSP_Init()
{
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_USART3_UART_Init();
    MX_LWIP_Init();
    MX_USB_HOST_Init();

    return BSP_OK;
}
