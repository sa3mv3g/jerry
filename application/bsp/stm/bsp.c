#include "bsp.h"
#include "stm32h5xx_hal.h"

void SystemClock_Config(void);

bsp_error_t BSP_Init()
{
    HAL_Init();
    SystemClock_Config();
    return BSP_OK;
}
