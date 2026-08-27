/**
 * @file audio_processing.c
 * @brief Implementation of fixed-point PCM conversion, gain, mixing and mute helpers.
 */
#include "audio_processing.h"
#include "requirements.h"

#include <limits.h>

_Static_assert(sizeof(int16_t) == 2U, "Application requires 16-bit int16_t");
_Static_assert(sizeof(int32_t) == 4U, "Application requires 32-bit int32_t");

/**
 * @brief Clamp a widened signed intermediate into the representable S16 PCM range.
 * @param value Signed intermediate value.
 * @return Saturated S16 sample.
 */
static int16_t saturate_i16(int64_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

/** @copydoc Audio_IsSupportedSampleRate */
bool Audio_IsSupportedSampleRate(uint32_t sample_rate_hz)
{
    return (sample_rate_hz == REQ_SAMPLE_RATE_16K_HZ) ||
           (sample_rate_hz == REQ_SAMPLE_RATE_48K_HZ);
}

/** @copydoc Audio_DecodeS24ToS16 */
int16_t Audio_DecodeS24ToS16(uint32_t word)
{
    uint32_t raw24 = word & 0x00FFFFFFUL;
    if ((raw24 & 0x00800000UL) != 0U) {
        raw24 |= 0xFF000000UL;
    }

    const int32_t signed24 = (int32_t)raw24;
    return (int16_t)(signed24 >> 8);
}

/** @copydoc Audio_ApplyGainQ15 */
int16_t Audio_ApplyGainQ15(int16_t sample, int32_t gain_q15)
{
    if (gain_q15 < 0) {
        return 0;
    }

    const int64_t scaled = (int64_t)sample * (int64_t)gain_q15;
    const int64_t rounded = (scaled >= 0) ? (scaled + 16384LL) : (scaled - 16384LL);
    return saturate_i16(rounded / 32768LL);
}

/** @copydoc Audio_MixStereoToMono */
int16_t Audio_MixStereoToMono(int16_t left, int16_t right)
{
    const int32_t sum = (int32_t)left + (int32_t)right;
    return (int16_t)(sum / 2);
}

/** @copydoc Audio_ApplyMute */
int16_t Audio_ApplyMute(int16_t sample, bool muted)
{
    return muted ? 0 : sample;
}

/** @copydoc Audio_ProcessStereoToMono */
bool Audio_ProcessStereoToMono(const int16_t *stereo,
                               int16_t *mono,
                               size_t frame_count,
                               int32_t gain_q15,
                               bool muted)
{
    if (gain_q15 < 0) {
        return false;
    }
    if (frame_count == 0U) {
        return true;
    }
    if ((stereo == NULL) || (mono == NULL)) {
        return false;
    }

    for (size_t i = 0U; i < frame_count; ++i) {
        const int16_t mixed = Audio_MixStereoToMono(stereo[2U * i], stereo[(2U * i) + 1U]);
        const int16_t gained = Audio_ApplyGainQ15(mixed, gain_q15);
        mono[i] = Audio_ApplyMute(gained, muted);
    }

    return true;
}
