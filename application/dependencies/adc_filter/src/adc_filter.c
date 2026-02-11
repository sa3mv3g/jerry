/**
 * @file adc_filter.c
 * @brief ADC digital filter implementation using CMSIS-DSP.
 *
 * This module implements digital filtering for ADC inputs using
 * CMSIS-DSP biquad cascade filters.
 */

#include "adc_filter.h"

#include <string.h>

#include "adc_filter_coefficients.h"

/*******************************************************************************
 * Private Functions
 ******************************************************************************/

/**
 * @brief Initialize a single filter channel.
 *
 * @param[in,out] channel Pointer to the channel structure.
 */
static void adc_filter_init_channel(adc_filter_channel_t *channel)
{
    /* Clear state buffer */
    (void)memset(channel->state, 0, sizeof(channel->state));

    /* Initialize CMSIS-DSP biquad cascade filter */
    arm_biquad_cascade_df1_init_f32(&channel->instance, ADC_FILTER_NUM_STAGES,
                                    adc_filter_coefficients, channel->state);

    channel->initialized = true;
}

/*******************************************************************************
 * Public Functions
 ******************************************************************************/

void adc_filter_init(adc_filter_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    /* Initialize each channel */
    for (uint8_t ch = 0U; ch < ADC_FILTER_NUM_CHANNELS; ch++)
    {
        adc_filter_init_channel(&ctx->channels[ch]);
    }
}

float32_t adc_filter_process_sample(adc_filter_context_t *ctx, uint8_t channel,
                                    float32_t input)
{
    float32_t output = 0.0f;

    if ((ctx == NULL) || (channel >= ADC_FILTER_NUM_CHANNELS))
    {
        return input; /* Return unfiltered on error */
    }

    if (!ctx->channels[channel].initialized)
    {
        return input; /* Return unfiltered if not initialized */
    }

    /* Process single sample using CMSIS-DSP
     * Note: arm_biquad_cascade_df1_f32 processes blocks, so we use block size 1
     */
    arm_biquad_cascade_df1_f32(&ctx->channels[channel].instance, &input,
                               &output, 1U);

    return output;
}

void adc_filter_process_block(adc_filter_context_t *ctx, uint8_t channel,
                              const float32_t *input, float32_t *output,
                              uint32_t block_size)
{
    if ((ctx == NULL) || (input == NULL) || (output == NULL))
    {
        return;
    }

    if (channel >= ADC_FILTER_NUM_CHANNELS)
    {
        return;
    }

    if (!ctx->channels[channel].initialized)
    {
        /* Copy input to output unfiltered if not initialized */
        (void)memcpy(output, input, block_size * sizeof(float32_t));
        return;
    }

    if (block_size == 0U)
    {
        return;
    }

    /* Process block using CMSIS-DSP */
    arm_biquad_cascade_df1_f32(&ctx->channels[channel].instance, input, output,
                               block_size);
}

void adc_filter_reset(adc_filter_context_t *ctx, uint8_t channel)
{
    if ((ctx == NULL) || (channel >= ADC_FILTER_NUM_CHANNELS))
    {
        return;
    }

    /* Clear state buffer */
    (void)memset(ctx->channels[channel].state, 0,
                 sizeof(ctx->channels[channel].state));

    /* Re-initialize the filter instance with cleared state */
    if (ctx->channels[channel].initialized)
    {
        arm_biquad_cascade_df1_init_f32(
            &ctx->channels[channel].instance, ADC_FILTER_NUM_STAGES,
            adc_filter_coefficients, ctx->channels[channel].state);
    }
}

void adc_filter_reset_all(adc_filter_context_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    for (uint8_t ch = 0U; ch < ADC_FILTER_NUM_CHANNELS; ch++)
    {
        adc_filter_reset(ctx, ch);
    }
}

bool adc_filter_is_initialized(const adc_filter_context_t *ctx, uint8_t channel)
{
    if ((ctx == NULL) || (channel >= ADC_FILTER_NUM_CHANNELS))
    {
        return false;
    }

    return ctx->channels[channel].initialized;
}

uint32_t adc_filter_get_num_stages(void) { return ADC_FILTER_NUM_STAGES; }

uint32_t adc_filter_get_sample_rate(void) { return ADC_FILTER_SAMPLE_RATE; }
