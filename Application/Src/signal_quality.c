/**
 * @file signal_quality.c
 * @brief Integer implementation of telecoil RMS ratio and 20 dB classification helpers.
 */
#include "signal_quality.h"

#include <limits.h>
#include <stddef.h>

/** @copydoc SignalQuality_Classify20dB */
SignalQualityResult_t SignalQuality_Classify20dB(uint32_t signal_rms,
                                                 uint32_t noise_rms,
                                                 SignalQualityClass_t *classification)
{
    if (classification == NULL) {
        return SIGNAL_QUALITY_ERR_ARGUMENT;
    }

    if (noise_rms == 0U) {
        *classification = (signal_rms == 0U) ? SIGNAL_QUALITY_UNKNOWN
                                             : SIGNAL_QUALITY_MEETS_20DB;
        return SIGNAL_QUALITY_OK;
    }

    /* 20*log10(S/N) >= 20 dB  <=>  S/N >= 10 for RMS amplitudes. */
    const uint64_t minimum_signal = (uint64_t)noise_rms * UINT64_C(10);
    *classification = ((uint64_t)signal_rms >= minimum_signal)
                    ? SIGNAL_QUALITY_MEETS_20DB
                    : SIGNAL_QUALITY_BELOW_20DB;
    return SIGNAL_QUALITY_OK;
}

/** @copydoc SignalQuality_AmplitudeRatioQ8 */
SignalQualityResult_t SignalQuality_AmplitudeRatioQ8(uint32_t signal_rms,
                                                    uint32_t noise_rms,
                                                    uint32_t *ratio_q8)
{
    if (ratio_q8 == NULL) {
        return SIGNAL_QUALITY_ERR_ARGUMENT;
    }

    if (noise_rms == 0U) {
        *ratio_q8 = (signal_rms == 0U) ? 0U : UINT32_MAX;
        return SIGNAL_QUALITY_OK;
    }

    const uint64_t scaled = (uint64_t)signal_rms * UINT64_C(256);
    const uint64_t ratio = scaled / noise_rms;
    *ratio_q8 = (ratio > UINT32_MAX) ? UINT32_MAX : (uint32_t)ratio;
    return SIGNAL_QUALITY_OK;
}
