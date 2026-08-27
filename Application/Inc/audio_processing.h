/**
 * @file audio_processing.h
 * @brief Small fixed-point PCM helpers shared by Bluetooth and telecoil processing.
 *
 * The functions in this module are intentionally hardware-independent. They validate
 * supported sample rates, convert signed 24-bit PCM to S16, apply saturating Q15 gain,
 * fold stereo to mono and enforce mute. Intermediate arithmetic is widened where needed
 * so that normal full-scale PCM operations cannot wrap before saturation.
 */
#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check whether a sample rate is supported by the subsystem requirements.
 * @param sample_rate_hz Sample rate in hertz.
 * @return true for 16 kHz or 48 kHz; false for every other value.
 */
bool Audio_IsSupportedSampleRate(uint32_t sample_rate_hz);

/**
 * @brief Convert signed 24-bit PCM stored in bits [23:0] to signed 16-bit PCM.
 *
 * The 24-bit value is sign-extended and shifted right by eight bits, retaining the
 * most-significant 16 audio bits. This matches the portable PCM transport path used
 * for 24-bit converter/I2S-style words.
 *
 * @param word Packed signed 24-bit PCM value in the low 24 bits.
 * @return Converted signed 16-bit PCM sample.
 */
int16_t Audio_DecodeS24ToS16(uint32_t word);

/**
 * @brief Apply a non-negative Q15 gain with rounding and S16 saturation.
 *
 * Q15 unity is represented by 32768. Intermediate multiplication uses 64-bit arithmetic
 * and the final value is saturated to INT16_MIN..INT16_MAX rather than allowed to wrap.
 * A negative gain is treated as invalid and safely returns zero.
 *
 * @param sample Input signed 16-bit PCM sample.
 * @param gain_q15 Non-negative gain in Q15 format.
 * @return Gained and saturated S16 sample, or zero for a negative gain.
 */
int16_t Audio_ApplyGainQ15(int16_t sample, int32_t gain_q15);

/**
 * @brief Fold one stereo frame to mono using the arithmetic mean 0.5L + 0.5R.
 * @param left Left-channel S16 sample.
 * @param right Right-channel S16 sample.
 * @return Mono S16 sample calculated with a 32-bit intermediate sum.
 */
int16_t Audio_MixStereoToMono(int16_t left, int16_t right);

/**
 * @brief Apply the final Boolean mute gate to one PCM sample.
 * @param sample Input S16 sample.
 * @param muted True to force silence.
 * @return Zero when muted; otherwise @p sample unchanged.
 */
int16_t Audio_ApplyMute(int16_t sample, bool muted);

/**
 * @brief Process interleaved stereo frames into gained, optionally muted mono PCM.
 *
 * Each frame is mixed to mono, processed by Audio_ApplyGainQ15(), then passed through
 * Audio_ApplyMute(). A zero-length block is accepted even when buffers are NULL.
 *
 * @param stereo Interleaved input samples `[L0,R0,L1,R1,...]`.
 * @param mono Output buffer with @p frame_count entries.
 * @param frame_count Number of stereo frames to process.
 * @param gain_q15 Non-negative Q15 gain; 32768 is unity.
 * @param muted True to force every output sample to zero.
 * @return true on success; false for invalid non-zero-length buffers or negative gain.
 */
bool Audio_ProcessStereoToMono(const int16_t *stereo,
                               int16_t *mono,
                               size_t frame_count,
                               int32_t gain_q15,
                               bool muted);

#ifdef __cplusplus
}
#endif

#endif
