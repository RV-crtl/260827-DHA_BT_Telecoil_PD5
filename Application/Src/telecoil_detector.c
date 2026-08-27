/**
 * @file telecoil_detector.c
 * @brief Implementation of integer telecoil metrics and hysteretic block qualification.
 */
#include "telecoil_detector.h"

#include <limits.h>

/**
 * @brief Calculate floor(sqrt(value)) using an integer restoring-square-root algorithm.
 * @param value Unsigned 64-bit radicand.
 * @return Integer square root, sufficient for the detector's RMS range.
 */
static uint32_t integer_sqrt_u64(uint64_t value)
{
    uint64_t bit = 1ULL << 62;
    uint64_t result = 0ULL;

    while (bit > value) {
        bit >>= 2;
    }

    while (bit != 0ULL) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return (uint32_t)result;
}

/**
 * @brief Validate threshold ordering, clipping bounds and hysteresis streak counts.
 * @param config Detector configuration to validate.
 * @return true when all fields form a usable configuration.
 */
static bool config_is_valid(const TelecoilDetectorConfig_t *config)
{
    return (config != NULL) &&
           (config->present_rms_threshold > config->absent_rms_threshold) &&
           (config->required_present_blocks > 0U) &&
           (config->required_absent_blocks > 0U) &&
           (config->clip_threshold > 0U) &&
           (config->max_clipped_per_mille <= 1000U);
}

/** @copydoc TelecoilDetector_DefaultConfig */
TelecoilDetectorConfig_t TelecoilDetector_DefaultConfig(void)
{
    TelecoilDetectorConfig_t config;
    config.present_rms_threshold = 900U;
    config.absent_rms_threshold = 600U;
    config.min_peak_to_peak = 1200U;
    config.clip_threshold = 32000U;
    config.max_clipped_per_mille = 100U;
    config.required_present_blocks = 2U;
    config.required_absent_blocks = 3U;
    return config;
}

/** @copydoc TelecoilDetector_Init */
TelecoilDetectorResult_t TelecoilDetector_Init(TelecoilDetector_t *detector,
                                                const TelecoilDetectorConfig_t *config)
{
    if (detector == NULL) {
        return TELECOIL_DETECTOR_ERR_ARGUMENT;
    }
    if (!config_is_valid(config)) {
        return TELECOIL_DETECTOR_ERR_CONFIG;
    }

    detector->config = *config;
    detector->valid_signal = false;
    detector->present_streak = 0U;
    detector->absent_streak = 0U;
    return TELECOIL_DETECTOR_OK;
}

/** @copydoc TelecoilDetector_Reset */
TelecoilDetectorResult_t TelecoilDetector_Reset(TelecoilDetector_t *detector)
{
    if (detector == NULL) {
        return TELECOIL_DETECTOR_ERR_ARGUMENT;
    }

    detector->valid_signal = false;
    detector->present_streak = 0U;
    detector->absent_streak = 0U;
    return TELECOIL_DETECTOR_OK;
}

/** @copydoc TelecoilDetector_Process */
TelecoilDetectorResult_t TelecoilDetector_Process(TelecoilDetector_t *detector,
                                                   const int16_t *samples,
                                                   size_t sample_count,
                                                   TelecoilBlockMetrics_t *metrics)
{
    if ((detector == NULL) || (metrics == NULL) ||
        ((samples == NULL) && (sample_count > 0U)) || (sample_count == 0U)) {
        return TELECOIL_DETECTOR_ERR_ARGUMENT;
    }

    int16_t min_sample = INT16_MAX;
    int16_t max_sample = INT16_MIN;
    uint64_t sum_abs = 0ULL;
    uint64_t sum_sq = 0ULL;
    uint32_t clipped = 0U;

    for (size_t i = 0U; i < sample_count; ++i) {
        const int32_t sample = samples[i];
        const uint32_t magnitude = (sample < 0) ? (uint32_t)(-(int64_t)sample) : (uint32_t)sample;

        if (samples[i] < min_sample) {
            min_sample = samples[i];
        }
        if (samples[i] > max_sample) {
            max_sample = samples[i];
        }

        sum_abs += magnitude;
        sum_sq += (uint64_t)((int64_t)sample * (int64_t)sample);
        if (magnitude >= detector->config.clip_threshold) {
            ++clipped;
        }
    }

    const uint32_t rms = integer_sqrt_u64(sum_sq / sample_count);
    const uint32_t mean_abs = (uint32_t)(sum_abs / sample_count);
    const uint32_t peak_to_peak = (uint32_t)((int32_t)max_sample - (int32_t)min_sample);
    const uint32_t clipped_per_mille = (uint32_t)(((uint64_t)clipped * 1000ULL) / sample_count);

    const bool clipping_ok = clipped_per_mille <= detector->config.max_clipped_per_mille;
    const bool strong_enough = (rms >= detector->config.present_rms_threshold) &&
                               (peak_to_peak >= detector->config.min_peak_to_peak);
    const bool clearly_absent = (rms <= detector->config.absent_rms_threshold) ||
                                (peak_to_peak < detector->config.min_peak_to_peak) || !clipping_ok;

    bool changed = false;
    bool raw_candidate_present = false;

    if (!detector->valid_signal) {
        raw_candidate_present = strong_enough && clipping_ok;
        if (raw_candidate_present) {
            if (detector->present_streak < UINT8_MAX) {
                ++detector->present_streak;
            }
            detector->absent_streak = 0U;
            if (detector->present_streak >= detector->config.required_present_blocks) {
                detector->valid_signal = true;
                detector->present_streak = 0U;
                changed = true;
            }
        } else {
            detector->present_streak = 0U;
        }
    } else {
        raw_candidate_present = !clearly_absent;
        if (clearly_absent) {
            if (detector->absent_streak < UINT8_MAX) {
                ++detector->absent_streak;
            }
            detector->present_streak = 0U;
            if (detector->absent_streak >= detector->config.required_absent_blocks) {
                detector->valid_signal = false;
                detector->absent_streak = 0U;
                changed = true;
            }
        } else {
            detector->absent_streak = 0U;
        }
    }

    metrics->rms = rms;
    metrics->mean_abs = mean_abs;
    metrics->peak_to_peak = peak_to_peak;
    metrics->clipped_samples = clipped;
    metrics->raw_candidate_present = raw_candidate_present;
    metrics->valid_signal = detector->valid_signal;
    metrics->state_changed = changed;
    return TELECOIL_DETECTOR_OK;
}
