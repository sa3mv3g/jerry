#ifndef BSP_H
#define BSP_H

/**
 * @brief BSP Error Codes
 */
typedef int bsp_error_t;

#define BSP_OK          ((bsp_error_t)0)
#define BSP_ERROR       ((bsp_error_t)1)
#define BSP_BUSY        ((bsp_error_t)2)
#define BSP_TIMEOUT     ((bsp_error_t)3)
#define BSP_INVALID_ARG ((bsp_error_t)4)

bsp_error_t BSP_Init();

#endif // BSP_H