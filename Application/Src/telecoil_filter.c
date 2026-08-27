/**
 * @file telecoil_filter.c
 * @brief Implementation of the two-stage direct-form-I telecoil conditioning filter.
 */
#include "telecoil_filter.h"


/*
 * Telecoil conditioning policy (PD2 requirements 1.5, 1.10 and 1.11):
 * two cascaded second-order sections provide speech-band conditioning for the
 * supported 16 kHz and 48 kHz sample rates.  Coefficients are precomputed so
 * the embedded path performs only deterministic multiply-accumulate work.
 * Software tests verify pass-band behaviour, stop-band attenuation, impulse
 * decay and state continuity.  The physical >=20 dB telecoil SNR requirement
 * remains a hardware/integration measurement; signal_quality.c only evaluates
 * the software-side RMS criterion once signal/noise estimates are available.
 */

/**
 * @brief Load one second-order-section coefficient set and clear its sample history.
 * @param stage Stage to configure.
 * @param b0 Feed-forward coefficient b0.
 * @param b1 Feed-forward coefficient b1.
 * @param b2 Feed-forward coefficient b2.
 * @param a1 Feedback coefficient a1 used with a negative sign in the recurrence.
 * @param a2 Feedback coefficient a2 used with a negative sign in the recurrence.
 */
static void set_stage(TelecoilBiquad_t *stage,
                      float b0, float b1, float b2, float a1, float a2)
{
    stage->b0 = b0;
    stage->b1 = b1;
    stage->b2 = b2;
    stage->a1 = a1;
    stage->a2 = a2;
    stage->x1 = 0.0f;
    stage->x2 = 0.0f;
    stage->y1 = 0.0f;
    stage->y2 = 0.0f;
}

/** @copydoc TelecoilFilter_Init */
bool TelecoilFilter_Init(TelecoilFilter_t *filter, uint32_t sample_rate_hz)
{
    if (filter == NULL) {
        return false;
    }

    *filter = (TelecoilFilter_t){0};
    filter->sample_rate_hz = sample_rate_hz;

    if (sample_rate_hz == 16000U) {
        set_stage(&filter->stage[0],
                  0.37827911f, 0.75655823f, 0.37827911f,
                  0.42877652f, 0.22427533f);
        set_stage(&filter->stage[1],
                  1.0f, -2.0f, 1.0f,
                  -1.83373199f, 0.84753178f);
    } else if (sample_rate_hz == 48000U) {
        set_stage(&filter->stage[0],
                  0.06510831f, 0.13021662f, 0.06510831f,
                  -1.17296298f, 0.44398134f);
        set_stage(&filter->stage[1],
                  1.0f, -2.0f, 1.0f,
                  -1.94510494f, 0.94677483f);
    } else {
        return false;
    }

    filter->initialised = true;
    return true;
}

/** @copydoc TelecoilFilter_Reset */
void TelecoilFilter_Reset(TelecoilFilter_t *filter)
{
    if (filter == NULL) {
        return;
    }

    for (size_t i = 0U; i < TELECOIL_FILTER_STAGE_COUNT; ++i) {
        filter->stage[i].x1 = 0.0f;
        filter->stage[i].x2 = 0.0f;
        filter->stage[i].y1 = 0.0f;
        filter->stage[i].y2 = 0.0f;
    }
}

/** @copydoc TelecoilFilter_ProcessSample */
float TelecoilFilter_ProcessSample(TelecoilFilter_t *filter, float input)
{
    if ((filter == NULL) || !filter->initialised) {
        return 0.0f;
    }

    float value = input;
    for (size_t i = 0U; i < TELECOIL_FILTER_STAGE_COUNT; ++i) {
        TelecoilBiquad_t *const s = &filter->stage[i];
        const float output = (s->b0 * value) + (s->b1 * s->x1) + (s->b2 * s->x2)
                           - (s->a1 * s->y1) - (s->a2 * s->y2);
        s->x2 = s->x1;
        s->x1 = value;
        s->y2 = s->y1;
        s->y1 = output;
        value = output;
    }

    return value;
}

/** @copydoc TelecoilFilter_ProcessBlock */
bool TelecoilFilter_ProcessBlock(TelecoilFilter_t *filter,
                                 const float *input,
                                 float *output,
                                 size_t sample_count)
{
    if ((filter == NULL) || !filter->initialised) {
        return false;
    }
    if (sample_count == 0U) {
        return true;
    }
    if ((input == NULL) || (output == NULL)) {
        return false;
    }

    for (size_t i = 0U; i < sample_count; ++i) {
        output[i] = TelecoilFilter_ProcessSample(filter, input[i]);
    }
    return true;
}
