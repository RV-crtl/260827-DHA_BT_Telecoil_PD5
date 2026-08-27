/**
 * @file telecoil_detector.h
 * @brief Block-based telecoil signal metrics and hysteretic presence detection.
 *
 * The detector calculates RMS, mean absolute magnitude, peak-to-peak amplitude and clipping
 * incidence using integer arithmetic. Separate present/absent thresholds and consecutive-block
 * counters prevent chatter near the decision boundary. These thresholds are calibration defaults,
 * not a claim of physical telecoil calibration; the top-level service adds a time-qualified loss
 * layer so fault timing does not depend on PCM block length.
 */
#ifndef TELECOIL_DETECTOR_H
#define TELECOIL_DETECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes returned by telecoil detector operations. */
typedef enum {
    TELECOIL_DETECTOR_OK = 0,
    TELECOIL_DETECTOR_ERR_ARGUMENT,
    TELECOIL_DETECTOR_ERR_CONFIG
} TelecoilDetectorResult_t;

/** Detection thresholds and consecutive-block hysteresis settings. */
typedef struct {
    uint32_t present_rms_threshold;  /**< RMS required to qualify a strong candidate. */
    uint32_t absent_rms_threshold;   /**< RMS at/below which the signal is clearly absent. */
    uint32_t min_peak_to_peak;       /**< Minimum dynamic range required for a valid signal. */
    uint16_t clip_threshold;         /**< Absolute sample magnitude counted as clipped. */
    uint16_t max_clipped_per_mille;  /**< Maximum clipping incidence in parts per thousand. */
    uint8_t required_present_blocks; /**< Strong blocks required to latch validity. */
    uint8_t required_absent_blocks;  /**< Clearly absent blocks required to clear raw validity. */
} TelecoilDetectorConfig_t;

/** Persistent state for telecoil signal qualification. */
typedef struct {
    TelecoilDetectorConfig_t config;
    bool valid_signal;
    uint8_t present_streak;
    uint8_t absent_streak;
} TelecoilDetector_t;

/** Metrics calculated from the most recently processed PCM block. */
typedef struct {
    uint32_t rms;
    uint32_t mean_abs;
    uint32_t peak_to_peak;
    uint32_t clipped_samples;
    bool raw_candidate_present;
    bool valid_signal;
    bool state_changed;
} TelecoilBlockMetrics_t;

/**
 * @brief Return conservative default detector thresholds and hysteresis counts.
 *
 * The numeric thresholds are software calibration defaults chosen for deterministic PD5
 * verification and are intentionally configurable for later measured hardware calibration.
 *
 * @return Complete valid detector configuration.
 */
TelecoilDetectorConfig_t TelecoilDetector_DefaultConfig(void);

/**
 * @brief Initialise a detector from a validated configuration.
 * @param detector Detector state to initialise.
 * @param config Configuration with present threshold greater than absent threshold and non-zero streak counts.
 * @return TELECOIL_DETECTOR_OK, or an argument/configuration error.
 */
TelecoilDetectorResult_t TelecoilDetector_Init(TelecoilDetector_t *detector,
                                                const TelecoilDetectorConfig_t *config);

/**
 * @brief Clear latched validity and consecutive-block history.
 * @param detector Detector state to reset.
 * @return TELECOIL_DETECTOR_OK or an argument error.
 */
TelecoilDetectorResult_t TelecoilDetector_Reset(TelecoilDetector_t *detector);

/**
 * @brief Analyse one S16 PCM block and update hysteretic signal validity.
 *
 * RMS is calculated from mean-square energy using an integer square root. `INT16_MIN` is
 * widened before absolute-value operations so its magnitude is representable. Excessive
 * clipping, low RMS or insufficient peak-to-peak excursion can classify a block as absent.
 *
 * @param detector Initialised detector.
 * @param samples Input block; required for a non-zero block.
 * @param sample_count Number of samples; must be greater than zero.
 * @param metrics Destination for block statistics and state-change information.
 * @return TELECOIL_DETECTOR_OK or an argument error.
 */
TelecoilDetectorResult_t TelecoilDetector_Process(TelecoilDetector_t *detector,
                                                   const int16_t *samples,
                                                   size_t sample_count,
                                                   TelecoilBlockMetrics_t *metrics);

#ifdef __cplusplus
}
#endif

#endif
