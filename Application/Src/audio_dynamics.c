/**
 * @file audio_dynamics.c
 * @brief Implementation of deterministic DC rejection, bounded AGC and peak limiting.
 */
#include "audio_dynamics.h"

#include <limits.h>

/**
 * @brief Validate all numeric relationships in an AudioDynamicsConfig_t.
 * @param config Configuration to validate.
 * @return true when coefficients, gain bounds and smoothing shifts are usable.
 */
static bool config_is_valid(const AudioDynamicsConfig_t *config)
{
    return (config != NULL) &&
           (config->dc_alpha_q15 >= 0) && (config->dc_alpha_q15 <= 32768) &&
           (config->target_peak > 0) && (config->target_peak <= INT16_MAX) &&
           (config->min_gain_q15 >= 0) &&
           (config->max_gain_q15 >= config->min_gain_q15) &&
           (config->attack_shift <= 15U) && (config->release_shift <= 15U);
}

/**
 * @brief Return an S16 sample magnitude using a widened signed intermediate.
 * @param value Signed 16-bit sample.
 * @return Absolute magnitude as an unsigned 32-bit value, including INT16_MIN -> 32768.
 */
static uint32_t abs_i16(int16_t value)
{
    return (value < 0) ? (uint32_t)(-(int32_t)value) : (uint32_t)value;
}

/**
 * @brief Move the current gain toward a target using power-of-two exponential smoothing.
 *
 * The update `current + ((target-current) >> shift)` is inexpensive on an MCU and gives
 * separate attack/release time behaviour without floating-point division.
 *
 * @param current Current Q15 gain.
 * @param target Desired Q15 gain.
 * @param shift Smoothing shift; zero applies the target immediately.
 * @return Updated Q15 gain.
 */
static int32_t smooth_gain(int32_t current, int32_t target, uint8_t shift)
{
    if (shift == 0U) {
        return target;
    }
    return current + ((target - current) >> shift);
}

/** @copydoc AudioDynamics_DefaultConfig */
AudioDynamicsConfig_t AudioDynamics_DefaultConfig(void)
{
    AudioDynamicsConfig_t config;
    config.dc_alpha_q15 = 32604; /* 32604/32768 ~= 0.995: low-cost DC rejection. */
    config.target_peak = 24000;
    config.min_gain_q15 = 8192;  /* 0.25x */
    config.max_gain_q15 = 65536; /* 2.0x */
    config.attack_shift = 1U;    /* Fast gain reduction after a loud block. */
    config.release_shift = 4U;   /* Slower recovery reduces audible pumping. */
    return config;
}

/** @copydoc AudioDynamics_Init */
AudioDynamicsResult_t AudioDynamics_Init(AudioDynamics_t *state,
                                         const AudioDynamicsConfig_t *config)
{
    if (state == NULL) {
        return AUDIO_DYNAMICS_ERR_ARGUMENT;
    }
    if (!config_is_valid(config)) {
        return AUDIO_DYNAMICS_ERR_CONFIG;
    }
    state->config = *config;
    state->dc_x_prev = 0;
    state->dc_y_prev = 0;
    state->current_gain_q15 = 32768;
    state->initialised = true;
    return AUDIO_DYNAMICS_OK;
}

/** @copydoc AudioDynamics_Reset */
AudioDynamicsResult_t AudioDynamics_Reset(AudioDynamics_t *state)
{
    if (state == NULL) {
        return AUDIO_DYNAMICS_ERR_ARGUMENT;
    }
    if (!state->initialised) {
        return AUDIO_DYNAMICS_ERR_STATE;
    }
    state->dc_x_prev = 0;
    state->dc_y_prev = 0;
    state->current_gain_q15 = 32768;
    return AUDIO_DYNAMICS_OK;
}

/** @copydoc AudioDynamics_LimitS16 */
int16_t AudioDynamics_LimitS16(int32_t sample, int32_t limit)
{
    if (limit <= 0) {
        return 0;
    }
    if (limit > INT16_MAX) {
        limit = INT16_MAX;
    }
    if (sample > limit) {
        return (int16_t)limit;
    }
    if (sample < -limit) {
        return (int16_t)-limit;
    }
    return (int16_t)sample;
}

/** @copydoc AudioDynamics_TargetGainQ15 */
int32_t AudioDynamics_TargetGainQ15(const AudioDynamicsConfig_t *config,
                                    uint32_t peak)
{
    if (!config_is_valid(config)) {
        return 0;
    }
    if (peak == 0U) {
        return config->max_gain_q15;
    }

    int64_t gain = ((int64_t)config->target_peak * 32768LL) / (int64_t)peak;
    if (gain < config->min_gain_q15) {
        gain = config->min_gain_q15;
    }
    if (gain > config->max_gain_q15) {
        gain = config->max_gain_q15;
    }
    return (int32_t)gain;
}

/** @copydoc AudioDynamics_ProcessBlock */
AudioDynamicsResult_t AudioDynamics_ProcessBlock(AudioDynamics_t *state,
                                                 const int16_t *input,
                                                 int16_t *output,
                                                 size_t sample_count,
                                                 bool muted)
{
    if (state == NULL) {
        return AUDIO_DYNAMICS_ERR_ARGUMENT;
    }
    if (!state->initialised) {
        return AUDIO_DYNAMICS_ERR_STATE;
    }
    if (sample_count == 0U) {
        return AUDIO_DYNAMICS_OK;
    }
    if ((input == NULL) || (output == NULL)) {
        return AUDIO_DYNAMICS_ERR_ARGUMENT;
    }

    uint32_t peak = 0U;
    for (size_t i = 0U; i < sample_count; ++i) {
        const uint32_t magnitude = abs_i16(input[i]);
        if (magnitude > peak) {
            peak = magnitude;
        }
    }

    const int32_t target = AudioDynamics_TargetGainQ15(&state->config, peak);
    const uint8_t shift = (target < state->current_gain_q15)
                        ? state->config.attack_shift
                        : state->config.release_shift;
    state->current_gain_q15 = smooth_gain(state->current_gain_q15, target, shift);

    for (size_t i = 0U; i < sample_count; ++i) {
        if (muted) {
            output[i] = 0;
            continue;
        }

        /* First-order DC blocker: y[n] = x[n] - x[n-1] + alpha*y[n-1]. */
        const int32_t x = input[i];
        const int64_t feedback = (int64_t)state->config.dc_alpha_q15 * state->dc_y_prev;
        const int32_t y = x - state->dc_x_prev + (int32_t)(feedback / 32768LL);
        state->dc_x_prev = x;
        state->dc_y_prev = y;

        const int64_t scaled = (int64_t)y * state->current_gain_q15;
        const int32_t sample = (int32_t)(scaled / 32768LL);
        output[i] = AudioDynamics_LimitS16(sample, state->config.target_peak);
    }
    return AUDIO_DYNAMICS_OK;
}
