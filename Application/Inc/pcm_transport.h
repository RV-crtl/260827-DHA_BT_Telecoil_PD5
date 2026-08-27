/**
 * @file pcm_transport.h
 * @brief Portable PCM framing helpers for a future converter/I2S integration boundary.
 *
 * The module preserves the useful 24-bit PCM and stereo framing knowledge from the earlier
 * prototype without containing SAI, DMA or HAL calls. It converts either packed little-endian
 * S24 bytes or low-24-bit channel words to the S16 representation used by the portable DSP.
 */
#ifndef PCM_TRANSPORT_H
#define PCM_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes returned by PCM transport conversion operations. */
typedef enum {
    PCM_TRANSPORT_OK = 0,
    PCM_TRANSPORT_ERR_ARGUMENT
} PcmTransportResult_t;

/**
 * @brief Convert one signed 24-bit PCM word in bits [23:0] to S16.
 * @param packed_s24 Low-24-bit signed PCM value.
 * @return Converted signed 16-bit PCM sample.
 */
int16_t PcmTransport_S24WordToS16(uint32_t packed_s24);

/**
 * @brief Decode contiguous little-endian three-byte S24 samples into S16 PCM.
 *
 * Each input sample is interpreted as `[LSB, middle, MSB]`. A zero sample count is accepted
 * as a no-op and permits NULL buffers.
 *
 * @param input Packed input buffer containing exactly three bytes per sample.
 * @param sample_count Number of S24 samples to decode.
 * @param output Output buffer with @p sample_count S16 entries.
 * @return PCM_TRANSPORT_OK, or PCM_TRANSPORT_ERR_ARGUMENT for invalid non-empty buffers.
 */
PcmTransportResult_t PcmTransport_DecodePackedS24Le(const uint8_t *input,
                                                     size_t sample_count,
                                                     int16_t *output);

/**
 * @brief Decode interleaved low-24-bit stereo words into separate S16 channels.
 * @param input_words Input words ordered `[L0,R0,L1,R1,...]`.
 * @param frame_count Number of stereo frames.
 * @param left Destination for left-channel S16 samples.
 * @param right Destination for right-channel S16 samples.
 * @return PCM_TRANSPORT_OK or an argument error.
 */
PcmTransportResult_t PcmTransport_DecodeStereoWords(const uint32_t *input_words,
                                                     size_t frame_count,
                                                     int16_t *left,
                                                     int16_t *right);

/**
 * @brief Decode interleaved S24 stereo words and fold each frame directly to mono S16.
 * @param input_words Input words ordered `[L0,R0,L1,R1,...]`.
 * @param frame_count Number of stereo frames.
 * @param mono Destination for @p frame_count mono samples.
 * @return PCM_TRANSPORT_OK or an argument error.
 */
PcmTransportResult_t PcmTransport_DecodeStereoWordsToMono(const uint32_t *input_words,
                                                           size_t frame_count,
                                                           int16_t *mono);

/**
 * @brief Duplicate mono S16 samples into an interleaved stereo S16 output stream.
 * @param mono Input mono buffer with @p frame_count samples.
 * @param frame_count Number of frames to generate.
 * @param stereo Destination ordered `[L0,R0,L1,R1,...]`, with L and R equal.
 * @return PCM_TRANSPORT_OK or an argument error.
 */
PcmTransportResult_t PcmTransport_MonoToStereoS16(const int16_t *mono,
                                                   size_t frame_count,
                                                   int16_t *stereo);

#ifdef __cplusplus
}
#endif

#endif
