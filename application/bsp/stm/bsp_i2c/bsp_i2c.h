/**
 * @file    bsp_i2c.h
 * @brief   Board Support Package for I2C Controller and Target management.
 * @details Provides the state machine definitions and thread-safe access
 * controls for I2C communication.
 */

#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "FreeRTOS.h"
#include "bsp.h"
#include "semphr.h"

/* ========================================================================== */
/*                 Public Definitions and Macros                              */
/* ========================================================================== */

/** * @name I2C Target Lifecycle States
 * @brief Represents the current operational state of a device on the I2C bus.
 * @{
 */
#define BSP_I2C_TARGET_STATE_UNCONFIG (0U)
#define BSP_I2C_TARGET_STATE_INIT     (1U)
#define BSP_I2C_TARGET_STATE_CONN     (2U)
#define BSP_I2C_TARGET_STATE_DISCON   (3U)
/** @} */

/* ==========================================================================
 */
/*                 Public Typedefs */
/* ==========================================================================
 */

/**
 * @struct BSP_I2C_Target_State_t
 * @brief Tracks the state of a specific target device on the I2C bus.
 */
typedef struct
{
    uint32_t presentState; /**< Current lifecycle state (e.g., CONN, DISCON) */
} BSP_I2C_Target_State_t;

/* ====================================================================*/
/*                 Public Functions */
/* ====================================================================*/

uint32_t BSP_I2C_GetBusStatus();

/**
 * @name I2C Controller Shared Bus Management
 * @brief Functions to manage the shared I2C bus cleanly across multiple
 * RTOS tasks.
 * @{
 */

/**
 * @brief Initializes the static FreeRTOS mutex for the I2C Controller.
 * @note This must be called during system initialization before the
 * scheduler starts.
 */
void BSP_I2C_Controller_MutexInit(void);

/**
 * @brief Acquires the I2C Controller mutex.
 * @details Blocks the calling task until the shared I2C bus is available.
 */
BaseType_t BSP_I2C_Controller_MutexLock(void);

/**
 * @brief Releases the I2C Controller mutex.
 * @details Frees the shared I2C bus for other waiting tasks.
 */
void BSP_I2C_Controller_MutexUnlock(void);

/** @} */

/**
 * @name I2C Target Device Management
 * @brief Functions to handle individual device (Target) connections and
 * states.
 * @{
 */

/**
 * @brief Initializes the target state struct to its default (UNCONFIG)
 * values.
 * @param[out] statePtr Pointer to the target state structure to reset.
 * @return bsp_error_t BSP_OK if successful, standard error code otherwise.
 */
bsp_error_t BSP_I2C_Target_ParamsInit(BSP_I2C_Target_State_t *statePtr);

/**
 * @brief Prepares the target software context (transitions to INIT state).
 * @param[in,out] statePtr Pointer to the target state structure.
 * @return bsp_error_t BSP_OK if initialized successfully.
 */
bsp_error_t BSP_I2C_Target_Init(BSP_I2C_Target_State_t *statePtr);

/**
 * @brief Polls the physical target device to verify its presence on the
 * bus.
 * @details Typically utilizes HAL_I2C_IsDeviceReady internally. Transitions
 * state to CONN if found.
 * @param[in,out] statePtr Pointer to the target state structure.
 * @return bsp_error_t BSP_OK if the device ACKs its address, error code if
 * missing.
 */
bsp_error_t BSP_I2C_Target_Connect(BSP_I2C_Target_State_t *statePtr);

/**
 * @brief Explicitly marks the target device as disconnected (transitions to
 * DISCON state).
 * @param[in,out] statePtr Pointer to the target state structure.
 * @return bsp_error_t BSP_OK if successfully updated.
 */
bsp_error_t BSP_I2C_Target_Disconnect(BSP_I2C_Target_State_t *statePtr);

/**
 * @brief Safely retrieves the current lifecycle state of the target device.
 * @param[in] statePtr Pointer to the target state structure being queried.
 * @param[out] valuePtr Pointer to the variable where the state value will
 * be written.
 * @return bsp_error_t BSP_OK if the read was successful.
 */
bsp_error_t BSP_I2C_Target_GetState(BSP_I2C_Target_State_t *statePtr,
                                    uint32_t               *valuePtr);

/** @} */

HAL_StatusTypeDef I2C_WriteData_Async(I2C_HandleTypeDef *hi2c,
                                      uint16_t DevAddress, uint8_t *pData,
                                      uint16_t Size);

#endif  // BSP_I2C_H