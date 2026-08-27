/**
 * @file pcm_transport.c
 * @brief Implementation of portable S24/S16 and stereo/mono PCM framing helpers.
 */
#include "pcm_transport.h"

#include "audio_processing.h"

/** @copydoc PcmTransport_S24WordToS16 */
int16_t PcmTransport_S24WordToS16(uint32_t packed_s24)
{
    return Audio_DecodeS24ToS16(packed_s24 & UINT32_C(0x00FFFFFF));
}

/** @copydoc PcmTransport_DecodePackedS24Le */
PcmTransportResult_t PcmTransport_DecodePackedS24Le(const uint8_t *input,
                                                     size_t sample_count,
                                                     int16_t *output)
{
    if (sample_count == 0U) {
        return PCM_TRANSPORT_OK;
    }
    if ((input == NULL) || (output == NULL)) {
        return PCM_TRANSPORT_ERR_ARGUMENT;
    }

    for (size_t i = 0U; i < sample_count; ++i) {
        const size_t j = i * 3U;
        const uint32_t word = (uint32_t)input[j] |
                              ((uint32_t)input[j + 1U] << 8U) |
                              ((uint32_t)input[j + 2U] << 16U);
        output[i] = PcmTransport_S24WordToS16(word);
    }
    return PCM_TRANSPORT_OK;
}

/** @copydoc PcmTransport_DecodeStereoWords */
PcmTransportResult_t PcmTransport_DecodeStereoWords(const uint32_t *input_words,
                                                     size_t frame_count,
                                                     int16_t *left,
                                                     int16_t *right)
{
    if (frame_count == 0U) {
        return PCM_TRANSPORT_OK;
    }
    if ((input_words == NULL) || (left == NULL) || (right == NULL)) {
        return PCM_TRANSPORT_ERR_ARGUMENT;
    }

    for (size_t i = 0U; i < frame_count; ++i) {
        left[i] = PcmTransport_S24WordToS16(input_words[2U * i]);
        right[i] = PcmTransport_S24WordToS16(input_words[(2U * i) + 1U]);
    }
    return PCM_TRANSPORT_OK;
}

/** @copydoc PcmTransport_DecodeStereoWordsToMono */
PcmTransportResult_t PcmTransport_DecodeStereoWordsToMono(const uint32_t *input_words,
                                                           size_t frame_count,
                                                           int16_t *mono)
{
    if (frame_count == 0U) {
        return PCM_TRANSPORT_OK;
    }
    if ((input_words == NULL) || (mono == NULL)) {
        return PCM_TRANSPORT_ERR_ARGUMENT;
    }

    for (size_t i = 0U; i < frame_count; ++i) {
        const int16_t left = PcmTransport_S24WordToS16(input_words[2U * i]);
        const int16_t right = PcmTransport_S24WordToS16(input_words[(2U * i) + 1U]);
        mono[i] = Audio_MixStereoToMono(left, right);
    }
    return PCM_TRANSPORT_OK;
}

/** @copydoc PcmTransport_MonoToStereoS16 */
PcmTransportResult_t PcmTransport_MonoToStereoS16(const int16_t *mono,
                                                   size_t frame_count,
                                                   int16_t *stereo)
{
    if (frame_count == 0U) {
        return PCM_TRANSPORT_OK;
    }
    if ((mono == NULL) || (stereo == NULL)) {
        return PCM_TRANSPORT_ERR_ARGUMENT;
    }

    for (size_t i = 0U; i < frame_count; ++i) {
        stereo[2U * i] = mono[i];
        stereo[(2U * i) + 1U] = mono[i];
    }
    return PCM_TRANSPORT_OK;
}
