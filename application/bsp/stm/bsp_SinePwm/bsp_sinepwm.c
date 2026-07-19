#include "bsp.h"
#include "main.h"
#include "math.h"

/* ========================================================================== */
/*                 Private Definitions and Macros                             */
/* ========================================================================== */
#define PWM_CHANNELS_CNTS    (4U)
#define SINE_WAVE_FREQ_HZ    (50U)
#define CARRIER_WAVE_FREQ_HZ (TIM3_PWM_BASE_FREQ_KHZ * 1000U)
#define CARRIER_WAVES_PER_SINE_WAVE_CNTS \
    ((uint32_t)(CARRIER_WAVE_FREQ_HZ / SINE_WAVE_FREQ_HZ))

/* ========================================================================== */
/*                 Private Typedefs                                           */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Function Prototype                                 */
/* ========================================================================== */

/* ========================================================================== */
/*                 Private Variable Declaration                               */
/* ========================================================================== */
static uint16_t s_calc_values_a[PWM_CHANNELS_CNTS]
                               [CARRIER_WAVES_PER_SINE_WAVE_CNTS] = {0};
static uint16_t s_calc_values_b[PWM_CHANNELS_CNTS]
                               [CARRIER_WAVES_PER_SINE_WAVE_CNTS] = {0};
static uint16_t *s_active_calc_values_ptr[PWM_CHANNELS_CNTS]      = {
    s_calc_values_a[0], s_calc_values_a[1], s_calc_values_a[2],
    s_calc_values_a[3]};

/* ========================================================================== */
/*                 Public Functions                                           */
/* ========================================================================== */

bsp_error_t BSP_SinePwm_Init(void)
{
    bsp_error_t              err = BSP_OK;
    extern TIM_HandleTypeDef Tim3_Handle;

    // Initialize all 4 channels with 0 amplitude and 0 phase
    for (uint8_t ch = 1; ch <= 4; ch++)
    {
        if (err == BSP_OK)
        {
            err = BSP_SinePwm_Update(ch, 0.0f, 0U);
        }
    }

    if (err == BSP_OK)
    {
        // Start the base timer
        HAL_TIM_Base_Start(&Tim3_Handle);
    }

    return err;
}

bsp_error_t BSP_SinePwm_Update(uint8_t channel, float amplitude,
                               uint16_t phase_deg)
{
    bsp_error_t              status = BSP_OK;
    extern TIM_HandleTypeDef Tim3_Handle;
    uint32_t                 timer_arr = __HAL_TIM_GET_AUTORELOAD(&Tim3_Handle);
    uint32_t                 tim_channel = 0;

    if (channel < 1 || channel > 4)
    {
        status = BSP_INVALID_ARG;
    }
    else
    {
        switch (channel)
        {
            case 1:
                tim_channel = TIM_CHANNEL_1;
                break;
            case 2:
                tim_channel = TIM_CHANNEL_2;
                break;
            case 3:
                tim_channel = TIM_CHANNEL_3;
                break;
            case 4:
                tim_channel = TIM_CHANNEL_4;
                break;
            default:
                break;
        }

        // Stop any ongoing PWM or DMA on this channel
        HAL_TIM_PWM_Stop_DMA(&Tim3_Handle, tim_channel);
        HAL_TIM_PWM_Stop(&Tim3_Handle, tim_channel);

        if (phase_deg == 0xFFFFU)
        {
            // Linear DC mode
            uint32_t duty = (uint32_t)(amplitude * (float)timer_arr);
            __HAL_TIM_SET_COMPARE(&Tim3_Handle, tim_channel, duty);
            HAL_TIM_PWM_Start(&Tim3_Handle, tim_channel);
        }
        else
        {
            // AC Sine wave mode
            uint8_t   ch_idx = channel - 1;
            uint16_t *nonactive_ptr;

            if (s_active_calc_values_ptr[ch_idx] == s_calc_values_a[ch_idx])
            {
                nonactive_ptr = s_calc_values_b[ch_idx];
            }
            else
            {
                nonactive_ptr = s_calc_values_a[ch_idx];
            }

            float phase_rad = ((float)phase_deg * (float)M_PI) / 180.0f;

            for (uint32_t i = 0; i < CARRIER_WAVES_PER_SINE_WAVE_CNTS; i++)
            {
                float angle =
                    (2.0f * (float)M_PI *
                     ((float)i / (float)CARRIER_WAVES_PER_SINE_WAVE_CNTS)) +
                    phase_rad;
                float sine_val   = (sinf(angle) * amplitude + 1.0f) * 0.5f;
                nonactive_ptr[i] = (uint32_t)(sine_val * (float)timer_arr);
            }

            s_active_calc_values_ptr[ch_idx] = nonactive_ptr;

            HAL_TIM_PWM_Start_DMA(&Tim3_Handle, tim_channel,
                                  (uint32_t *)s_active_calc_values_ptr[ch_idx],
                                  CARRIER_WAVES_PER_SINE_WAVE_CNTS);
        }
    }

    return status;
}