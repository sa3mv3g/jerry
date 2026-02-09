/**
 * @file adc_filter.h
 * @brief ADC digital filter API using CMSIS-DSP biquad cascade filters.
 *
 * This module provides digital filtering for ADC inputs with:
 * - 4th order Butterworth low-pass filter (500Hz cutoff)
 * - 10 IIR notch filters for 50Hz mains rejection and harmonics
 *
 * The filter uses CMSIS-DSP arm_biquad_cascade_df1_f32 for efficient
 * implementation on Cortex-M processors.
 *
 * @note All memory is statically allocated - no dynamic allocation.
 *
 * Usage:
 * @code
 *   adc_filter_context_t filter_ctx;
 *   adc_filter_init(&filter_ctx);
 *
 *   // Process samples
 *   float32_t filtered = adc_filter_process_sample(&filter_ctx, 0, raw_sample);
 * @endcode
 */

#ifndef ADC_FILTER_H
#define ADC_FILTER_H

#include <stdbool.h>
#include <stdint.h>

#include "adc_filter_coefficients.h"
#include "arm_math.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/** Number of ADC channels supported */
#define ADC_FILTER_NUM_CHANNELS 6U

/** Maximum block size for block processing */
#define ADC_FILTER_MAX_BLOCK_SIZE 64U

    /*******************************************************************************
     * Types
     ******************************************************************************/

    /**
     * @brief Filter instance for a single ADC channel.
     *
     * Contains the CMSIS-DSP biquad instance and state buffer.
     * Each channel has independent state for proper filtering.
     */
    typedef struct
    {
        /** CMSIS-DSP biquad cascade filter instance */
        arm_biquad_casd_df1_inst_f32 instance;

        /** State buffer for the filter (4 values per stage) */
        float32_t state[ADC_FILTER_STATE_SIZE];

        /** Flag indicating if the channel is initialized */
        bool initialized;
    } adc_filter_channel_t;

    /**
     * @brief Filter context containing all ADC channels.
     *
     * This structure holds the filter instances for all channels.
     * Coefficients are shared across all channels.
     */
    typedef struct
    {
        /** Filter instances for each channel */
        adc_filter_channel_t channels[ADC_FILTER_NUM_CHANNELS];
    } adc_filter_context_t;

    /*******************************************************************************
     * API Functions
     ******************************************************************************/

    /**
     * @brief Initialize all filter channels.
     *
     * This function initializes the CMSIS-DSP biquad cascade filter for each
     * ADC channel. All channels share the same coefficients but have
     * independent state buffers.
     *
     * @param[in,out] ctx Pointer to the filter context to initialize.
     *                    Must not be NULL.
     *
     * @note This function must be called before any filtering operations.
     * @note All state buffers are cleared to zero.
     */
    void adc_filter_init(adc_filter_context_t *ctx);

    /**
     * @brief Process a single sample for one channel.
     *
     * Applies the complete filter chain (LPF + notch filters) to a single
     * input sample and returns the filtered output.
     *
     * @param[in,out] ctx     Pointer to the filter context.
     * @param[in]     channel Channel index (0 to ADC_FILTER_NUM_CHANNELS-1).
     * @param[in]     input   Input sample value.
     *
     * @return Filtered output sample.
     *
     * @note The channel must be initialized before calling this function.
     * @note For better efficiency, consider using adc_filter_process_block()
     *       when processing multiple samples.
     */
    float32_t adc_filter_process_sample(adc_filter_context_t *ctx,
                                        uint8_t channel, float32_t input);

    /**
     * @brief Process a block of samples for one channel.
     *
     * Applies the complete filter chain to a block of input samples.
     * This is more efficient than processing samples individually.
     *
     * @param[in,out] ctx        Pointer to the filter context.
     * @param[in]     channel    Channel index (0 to ADC_FILTER_NUM_CHANNELS-1).
     * @param[in]     input      Pointer to input sample buffer.
     * @param[out]    output     Pointer to output sample buffer.
     * @param[in]     block_size Number of samples to process.
     *
     * @note Input and output buffers must not overlap.
     * @note block_size should not exceed ADC_FILTER_MAX_BLOCK_SIZE.
     */
    void adc_filter_process_block(adc_filter_context_t *ctx, uint8_t channel,
                                  const float32_t *input, float32_t *output,
                                  uint32_t block_size);

    /**
     * @brief Reset filter state for a single channel.
     *
     * Clears the state buffer for the specified channel, effectively
     * resetting the filter to its initial state. This is useful when
     * there's a discontinuity in the input signal.
     *
     * @param[in,out] ctx     Pointer to the filter context.
     * @param[in]     channel Channel index (0 to ADC_FILTER_NUM_CHANNELS-1).
     */
    void adc_filter_reset(adc_filter_context_t *ctx, uint8_t channel);

    /**
     * @brief Reset filter state for all channels.
     *
     * Clears the state buffers for all channels.
     *
     * @param[in,out] ctx Pointer to the filter context.
     */
    void adc_filter_reset_all(adc_filter_context_t *ctx);

    /**
     * @brief Check if a channel is initialized.
     *
     * @param[in] ctx     Pointer to the filter context.
     * @param[in] channel Channel index (0 to ADC_FILTER_NUM_CHANNELS-1).
     *
     * @return true if the channel is initialized, false otherwise.
     */
    bool adc_filter_is_initialized(const adc_filter_context_t *ctx,
                                   uint8_t                     channel);

    /**
     * @brief Get the number of filter stages.
     *
     * @return Number of biquad stages in the filter cascade.
     */
    uint32_t adc_filter_get_num_stages(void);

    /**
     * @brief Get the filter sample rate.
     *
     * @return Sample rate in Hz that the filter was designed for.
     */
    uint32_t adc_filter_get_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_FILTER_H */
