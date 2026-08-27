/**
 * @file audio_dynamics.h
 * @brief Hardware-independent audio dynamics processing for the hearing-aid path.
 *
 * The module provides a small deterministic signal-conditioning stage that can be
 * exercised on the STM32F446RE or on a host PC without any peripheral drivers.
 * It combines a first-order DC blocker, block-based peak estimation, bounded Q15
 * automatic gain control (AGC), attack/release gain smoothing, peak limiting and
 * a final mute gate.
 *
 * The DC blocker implements the recurrence
 * `y[n] = x[n] - x[n-1] + alpha*y[n-1]`, where `alpha` is stored in Q15 format.
 * AGC target gain is calculated from `target_peak / measured_peak` and clamped to
 * configured minimum and maximum gains before smoothing. No heap allocation or
 * floating-point library functions are required.
 */
#ifndef AUDIO_DYNAMICS_H
#define AUDIO_DYNAMICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes returned by audio-dynamics operations. */
typedef enum {
    AUDIO_DYNAMICS_OK = 0,
    AUDIO_DYNAMICS_ERR_ARGUMENT,
    AUDIO_DYNAMICS_ERR_CONFIG,
    AUDIO_DYNAMICS_ERR_STATE
} AudioDynamicsResult_t;

/** Configuration for the DC blocker, bounded AGC and limiter. */
typedef struct {
    int32_t dc_alpha_q15;   /**< DC-blocker feedback coefficient; 32768 represents 1.0. */
    int32_t target_peak;    /**< Desired absolute PCM peak after gain/limiting. */
    int32_t min_gain_q15;   /**< Minimum allowed gain in Q15. */
    int32_t max_gain_q15;   /**< Maximum allowed gain in Q15. */
    uint8_t attack_shift;   /**< Right-shift used when reducing gain; lower is faster. */
    uint8_t release_shift;  /**< Right-shift used when increasing gain; higher is slower. */
} AudioDynamicsConfig_t;

/** Persistent processing state for one audio-dynamics instance. */
typedef struct {
    AudioDynamicsConfig_t config; /**< Validated processing configuration. */
    int32_t dc_x_prev;            /**< Previous DC-blocker input sample. */
    int32_t dc_y_prev;            /**< Previous DC-blocker output sample. */
    int32_t current_gain_q15;     /**< Smoothed gain currently applied to samples. */
    bool initialised;             /**< True after successful AudioDynamics_Init(). */
} AudioDynamics_t;

/**
 * @brief Return the default dynamics configuration used by the portable audio path.
 *
 * The defaults use an approximately 0.995 DC-blocker coefficient, a 24000-count
 * target peak, a 0.25x-to-2.0x gain range, faster attenuation and slower recovery.
 *
 * @return A complete valid configuration suitable for AudioDynamics_Init().
 */
AudioDynamicsConfig_t AudioDynamics_DefaultConfig(void);

/**
 * @brief Initialise an audio-dynamics state object from a validated configuration.
 *
 * @param state State object to initialise.
 * @param config Configuration containing valid gain, limiter and smoothing values.
 * @return AUDIO_DYNAMICS_OK on success, otherwise an argument or configuration error.
 */
AudioDynamicsResult_t AudioDynamics_Init(AudioDynamics_t *state,
                                         const AudioDynamicsConfig_t *config);

/**
 * @brief Reset processing history while preserving the validated configuration.
 *
 * DC history is cleared and gain returns to unity (32768 in Q15).
 *
 * @param state Initialised dynamics state.
 * @return AUDIO_DYNAMICS_OK on success, or an argument/state error.
 */
AudioDynamicsResult_t AudioDynamics_Reset(AudioDynamics_t *state);

/**
 * @brief Clamp a signed intermediate sample to a symmetric S16 peak limit.
 *
 * A non-positive limit returns zero. Limits above INT16_MAX are clipped to the
 * representable S16 range before the sample is saturated.
 *
 * @param sample Signed intermediate sample to limit.
 * @param limit Positive absolute limit in S16 counts.
 * @return Saturated signed 16-bit sample.
 */
int16_t AudioDynamics_LimitS16(int32_t sample, int32_t limit);

/**
 * @brief Calculate the bounded Q15 gain required to move a measured peak toward target_peak.
 *
 * For a non-zero peak, the unsmoothed gain is
 * `target_peak * 32768 / peak`, then clamped to the configured minimum/maximum.
 * A zero peak requests the maximum configured gain.
 *
 * @param config Valid dynamics configuration.
 * @param peak Absolute block peak in S16 counts.
 * @return Target gain in Q15, or zero when the configuration is invalid.
 */
int32_t AudioDynamics_TargetGainQ15(const AudioDynamicsConfig_t *config,
                                    uint32_t peak);

/**
 * @brief Process one mono S16 block through DC rejection, AGC, limiting and mute.
 *
 * Peak is measured once per block, the target gain is smoothed using the attack or
 * release shift, and state is retained across calls. When @p muted is true the output
 * block is forced to zero; the function remains deterministic and performs no allocation.
 *
 * @param state Initialised dynamics state.
 * @param input Input mono PCM samples; required when @p sample_count is non-zero.
 * @param output Output buffer with space for @p sample_count samples.
 * @param sample_count Number of samples to process. Zero is accepted as a no-op.
 * @param muted True to force every output sample to zero.
 * @return AUDIO_DYNAMICS_OK on success, otherwise an argument/state error.
 */
AudioDynamicsResult_t AudioDynamics_ProcessBlock(AudioDynamics_t *state,
                                                 const int16_t *input,
                                                 int16_t *output,
                                                 size_t sample_count,
                                                 bool muted);

#ifdef __cplusplus
}
#endif

#endif
