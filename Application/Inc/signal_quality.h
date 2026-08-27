/**
 * @file signal_quality.h
 * @brief Integer/fixed-point telecoil signal-quality helpers.
 *
 * The module evaluates the software-side form of the 20 dB telecoil SNR requirement from
 * caller-supplied RMS amplitudes. For amplitude quantities, `20*log10(S/N) >= 20 dB` is
 * equivalent to `S/N >= 10`, so classification can be performed exactly with integer
 * multiplication instead of `log10()` or a floating-point math-library dependency.
 */
#ifndef SIGNAL_QUALITY_H
#define SIGNAL_QUALITY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes for signal-quality calculations. */
typedef enum {
    SIGNAL_QUALITY_OK = 0,
    SIGNAL_QUALITY_ERR_ARGUMENT
} SignalQualityResult_t;

/** Qualitative result used by telecoil diagnostics. */
typedef enum {
    SIGNAL_QUALITY_UNKNOWN = 0,
    SIGNAL_QUALITY_BELOW_20DB,
    SIGNAL_QUALITY_MEETS_20DB
} SignalQualityClass_t;

/**
 * @brief Classify whether RMS signal/noise amplitudes satisfy the 20 dB criterion.
 *
 * A zero noise floor with non-zero signal is treated as meeting the threshold; zero signal
 * and zero noise is reported as UNKNOWN because no meaningful ratio can be inferred.
 * 64-bit arithmetic prevents overflow in the `noise_rms * 10` comparison.
 *
 * @param signal_rms RMS signal amplitude.
 * @param noise_rms RMS noise amplitude measured in the same units.
 * @param classification Destination for the qualitative result.
 * @return SIGNAL_QUALITY_OK or SIGNAL_QUALITY_ERR_ARGUMENT for a NULL destination.
 */
SignalQualityResult_t SignalQuality_Classify20dB(uint32_t signal_rms,
                                                 uint32_t noise_rms,
                                                 SignalQualityClass_t *classification);

/**
 * @brief Calculate the signal/noise amplitude ratio in Q8 fixed-point format.
 *
 * Q8 uses 256 to represent 1.0, so the returned value is
 * `signal_rms * 256 / noise_rms`. The result saturates at UINT32_MAX when the
 * mathematical ratio exceeds the representable range.
 *
 * @param signal_rms RMS signal amplitude.
 * @param noise_rms RMS noise amplitude in the same units.
 * @param ratio_q8 Destination for the Q8 ratio.
 * @return SIGNAL_QUALITY_OK or SIGNAL_QUALITY_ERR_ARGUMENT.
 */
SignalQualityResult_t SignalQuality_AmplitudeRatioQ8(uint32_t signal_rms,
                                                     uint32_t noise_rms,
                                                     uint32_t *ratio_q8);

#ifdef __cplusplus
}
#endif

#endif
