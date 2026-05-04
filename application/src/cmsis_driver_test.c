/*
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include "Driver_GPIO.h"

/* Test GPIO pin - using PA0 (Port A, Pin 0) */
#define TEST_GPIO_PIN (0U * 16U + 0U) /* Port A = 0, Pin 0 = 0 */

int main(void)
{
    int32_t status;
    int32_t pin_value;

    /* Initialize GPIO driver */
    status = Driver_GPIO0.Initialize(NULL);
    if (status != ARM_DRIVER_OK)
    {
        printf("GPIO Initialize failed: %d\r\n", status);
        return -1;
    }

    /* Power up GPIO driver */
    status = Driver_GPIO0.PowerControl(ARM_POWER_FULL);
    if (status != ARM_DRIVER_OK)
    {
        printf("GPIO PowerControl failed: %d\r\n", status);
        return -1;
    }

    /* Configure PA0 as output */
    status = Driver_GPIO0.PinConfigure(TEST_GPIO_PIN,
                                       ARM_GPIO_OUTPUT,      /* direction */
                                       ARM_GPIO_PUSH_PULL,   /* mode */
                                       ARM_GPIO_PULL_NONE,   /* pull */
                                       ARM_GPIO_TRIGGER_NONE /* event */
    );
    if (status != ARM_DRIVER_OK)
    {
        printf("GPIO PinConfigure failed: %d\r\n", status);
        return -1;
    }

    printf("CMSIS-Driver GPIO test started\r\n");

    /* Toggle the GPIO pin 5 times */
    for (int i = 0; i < 5; i++)
    {
        /* Set pin high */
        status = Driver_GPIO0.PinWrite(TEST_GPIO_PIN, 1);
        if (status != ARM_DRIVER_OK)
        {
            printf("GPIO PinWrite (high) failed: %d\r\n", status);
            return -1;
        }

        /* Read back pin value */
        pin_value = Driver_GPIO0.PinRead(TEST_GPIO_PIN);
        printf("GPIO Pin %d: %d\r\n", TEST_GPIO_PIN, pin_value);

        /* Set pin low */
        status = Driver_GPIO0.PinWrite(TEST_GPIO_PIN, 0);
        if (status != ARM_DRIVER_OK)
        {
            printf("GPIO PinWrite (low) failed: %d\r\n", status);
            return -1;
        }

        /* Read back pin value */
        pin_value = Driver_GPIO0.PinRead(TEST_GPIO_PIN);
        printf("GPIO Pin %d: %d\r\n", TEST_GPIO_PIN, pin_value);
    }

    /* De-initialize GPIO driver */
    status = Driver_GPIO0.Uninitialize();
    if (status != ARM_DRIVER_OK)
    {
        printf("GPIO Uninitialize failed: %d\r\n", status);
        return -1;
    }

    printf("CMSIS-Driver GPIO test completed successfully\r\n");
    return 0;
}