/**
 * @file telecoil_filter.h
 * @brief Stateful two-section telecoil speech-band conditioning filter.
 *
 * The filter uses two cascaded second-order IIR sections (biquads) with precomputed
 * coefficient sets for 16 kHz and 48 kHz operation. Each section implements the
 * direct-form-I recurrence
 * `y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]`.
 *
 * The coefficients are design constants selected to provide useful speech-band conditioning
 * around the 300 Hz to 5 kHz region. The module deliberately keeps coefficient generation
 * outside the runtime path; later measured calibration can replace the constants without
 * changing the API or state-machine design.
 */
#ifndef TELECOIL_FILTER_H
#define TELECOIL_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of cascaded second-order sections. */
#define TELECOIL_FILTER_STAGE_COUNT 2U

/** One direct-form-I biquad section and its persistent sample history. */
typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
} TelecoilBiquad_t;

/** Complete two-stage telecoil conditioning filter instance. */
typedef struct {
    uint32_t sample_rate_hz;
    TelecoilBiquad_t stage[TELECOIL_FILTER_STAGE_COUNT];
    bool initialised;
} TelecoilFilter_t;

/**
 * @brief Initialise the two-section filter for a supported sample rate.
 *
 * Separate precomputed coefficient sets are loaded for 16 kHz and 48 kHz. Unsupported
 * rates are rejected and leave the filter uninitialised.
 *
 * @param filter Filter instance to initialise.
 * @param sample_rate_hz Required sample rate in hertz.
 * @return true for a successful 16 kHz/48 kHz initialisation; otherwise false.
 */
bool TelecoilFilter_Init(TelecoilFilter_t *filter, uint32_t sample_rate_hz);

/**
 * @brief Clear biquad sample history while preserving coefficients and sample rate.
 * @param filter Filter instance. NULL is accepted as a no-op.
 */
void TelecoilFilter_Reset(TelecoilFilter_t *filter);

/**
 * @brief Process one floating-point sample through both cascaded biquads.
 * @param filter Initialised filter instance.
 * @param input Current input sample.
 * @return Filtered output sample, or 0.0f when the filter is NULL/uninitialised.
 */
float TelecoilFilter_ProcessSample(TelecoilFilter_t *filter, float input);

/**
 * @brief Process a contiguous block while preserving filter history between calls.
 * @param filter Initialised filter instance.
 * @param input Input sample buffer; required for a non-zero block.
 * @param output Output buffer with @p sample_count entries.
 * @param sample_count Number of samples. Zero is accepted as a no-op.
 * @return true on success; false for invalid state or non-empty NULL buffers.
 */
bool TelecoilFilter_ProcessBlock(TelecoilFilter_t *filter,
                                 const float *input,
                                 float *output,
                                 size_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
