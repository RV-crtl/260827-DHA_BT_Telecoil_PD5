#include "pcm_transport.h"
#include "unity.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static void test_pcm_word_zero(void)
{
    TEST_ASSERT_EQUAL_INT16(0, PcmTransport_S24WordToS16(0U));
}

static void test_pcm_word_max_positive(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, PcmTransport_S24WordToS16(UINT32_C(0x007FFFFF)));
}

static void test_pcm_word_min_negative(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, PcmTransport_S24WordToS16(UINT32_C(0x00800000)));
}

static void test_pcm_word_negative_one(void)
{
    TEST_ASSERT_EQUAL_INT16(-1, PcmTransport_S24WordToS16(UINT32_C(0x00FFFFFF)));
}

static void test_pcm_word_ignores_high_byte(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, PcmTransport_S24WordToS16(UINT32_C(0xAA7FFFFF)));
}

static void test_packed_zero_length_allows_null(void)
{
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK, PcmTransport_DecodePackedS24Le(NULL, 0U, NULL));
}

static void test_packed_rejects_null_input(void)
{
    int16_t output[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT,
                          PcmTransport_DecodePackedS24Le(NULL, 1U, output));
}

static void test_packed_rejects_null_output(void)
{
    const uint8_t input[3] = {0U, 0U, 0U};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT,
                          PcmTransport_DecodePackedS24Le(input, 1U, NULL));
}

static void test_packed_decodes_positive_little_endian(void)
{
    const uint8_t input[3] = {0x00U, 0x40U, 0x00U};
    int16_t output[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK,
                          PcmTransport_DecodePackedS24Le(input, 1U, output));
    TEST_ASSERT_EQUAL_INT16(64, output[0]);
}

static void test_packed_decodes_negative_little_endian(void)
{
    const uint8_t input[3] = {0x00U, 0x00U, 0xFFU};
    int16_t output[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK,
                          PcmTransport_DecodePackedS24Le(input, 1U, output));
    TEST_ASSERT_EQUAL_INT16(-256, output[0]);
}

static void test_packed_decodes_multiple_samples(void)
{
    const uint8_t input[6] = {0x00U, 0x01U, 0x00U, 0x00U, 0xFFU, 0xFFU};
    int16_t output[2] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK,
                          PcmTransport_DecodePackedS24Le(input, 2U, output));
    TEST_ASSERT_EQUAL_INT16(1, output[0]);
    TEST_ASSERT_EQUAL_INT16(-1, output[1]);
}

static void test_stereo_zero_length_allows_null(void)
{
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK, PcmTransport_DecodeStereoWords(NULL, 0U, NULL, NULL));
}

static void test_stereo_rejects_null_words(void)
{
    int16_t left[1] = {0};
    int16_t right[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT,
                          PcmTransport_DecodeStereoWords(NULL, 1U, left, right));
}

static void test_stereo_rejects_null_left(void)
{
    const uint32_t words[2] = {0U, 0U};
    int16_t right[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT,
                          PcmTransport_DecodeStereoWords(words, 1U, NULL, right));
}

static void test_stereo_rejects_null_right(void)
{
    const uint32_t words[2] = {0U, 0U};
    int16_t left[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT,
                          PcmTransport_DecodeStereoWords(words, 1U, left, NULL));
}

static void test_stereo_decodes_channel_order(void)
{
    const uint32_t words[4] = {0x000100U, 0x000200U, 0xFFFF00U, 0xFFFE00U};
    int16_t left[2] = {0};
    int16_t right[2] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK,
                          PcmTransport_DecodeStereoWords(words, 2U, left, right));
    TEST_ASSERT_EQUAL_INT16(1, left[0]);
    TEST_ASSERT_EQUAL_INT16(2, right[0]);
    TEST_ASSERT_EQUAL_INT16(-1, left[1]);
    TEST_ASSERT_EQUAL_INT16(-2, right[1]);
}

static void test_stereo_to_mono_zero_length_allows_null(void)
{
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK,
                          PcmTransport_DecodeStereoWordsToMono(NULL, 0U, NULL));
}

static void test_stereo_to_mono_rejects_null(void)
{
    const uint32_t words[2] = {0U, 0U};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT,
                          PcmTransport_DecodeStereoWordsToMono(words, 1U, NULL));
}

static void test_stereo_to_mono_folds_channels(void)
{
    const uint32_t words[2] = {0x000400U, 0x000200U};
    int16_t mono[1] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK,
                          PcmTransport_DecodeStereoWordsToMono(words, 1U, mono));
    TEST_ASSERT_EQUAL_INT16(3, mono[0]);
}

static void test_mono_to_stereo_duplicates_frames(void)
{
    const int16_t mono[2] = {123, -456};
    int16_t stereo[4] = {0};
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK, PcmTransport_MonoToStereoS16(NULL, 0U, NULL));
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT, PcmTransport_MonoToStereoS16(NULL, 1U, stereo));
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_ERR_ARGUMENT, PcmTransport_MonoToStereoS16(mono, 1U, NULL));
    TEST_ASSERT_EQUAL_INT(PCM_TRANSPORT_OK, PcmTransport_MonoToStereoS16(mono, 2U, stereo));
    TEST_ASSERT_EQUAL_INT16(123, stereo[0]);
    TEST_ASSERT_EQUAL_INT16(123, stereo[1]);
    TEST_ASSERT_EQUAL_INT16(-456, stereo[2]);
    TEST_ASSERT_EQUAL_INT16(-456, stereo[3]);
}

void RunPcmTransportTests(void)
{
    RUN_TEST(test_pcm_word_zero);
    RUN_TEST(test_pcm_word_max_positive);
    RUN_TEST(test_pcm_word_min_negative);
    RUN_TEST(test_pcm_word_negative_one);
    RUN_TEST(test_pcm_word_ignores_high_byte);
    RUN_TEST(test_packed_zero_length_allows_null);
    RUN_TEST(test_packed_rejects_null_input);
    RUN_TEST(test_packed_rejects_null_output);
    RUN_TEST(test_packed_decodes_positive_little_endian);
    RUN_TEST(test_packed_decodes_negative_little_endian);
    RUN_TEST(test_packed_decodes_multiple_samples);
    RUN_TEST(test_stereo_zero_length_allows_null);
    RUN_TEST(test_stereo_rejects_null_words);
    RUN_TEST(test_stereo_rejects_null_left);
    RUN_TEST(test_stereo_rejects_null_right);
    RUN_TEST(test_stereo_decodes_channel_order);
    RUN_TEST(test_stereo_to_mono_zero_length_allows_null);
    RUN_TEST(test_stereo_to_mono_rejects_null);
    RUN_TEST(test_stereo_to_mono_folds_channels);
    RUN_TEST(test_mono_to_stereo_duplicates_frames);
}
