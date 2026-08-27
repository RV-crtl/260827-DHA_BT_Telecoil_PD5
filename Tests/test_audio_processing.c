#include "audio_processing.h"
#include "unity.h"

#include <limits.h>

static void test_sample_rate_accepts_16k(void)
{
    TEST_ASSERT_TRUE(Audio_IsSupportedSampleRate(16000U));
}

static void test_sample_rate_accepts_48k(void)
{
    TEST_ASSERT_TRUE(Audio_IsSupportedSampleRate(48000U));
}

static void test_sample_rate_rejects_44k1(void)
{
    TEST_ASSERT_FALSE(Audio_IsSupportedSampleRate(44100U));
}

static void test_decode_24bit_zero(void)
{
    TEST_ASSERT_EQUAL_INT16(0, Audio_DecodeS24ToS16(0x00000000UL));
}

static void test_decode_24bit_max_positive(void)
{
    TEST_ASSERT_EQUAL_INT16(32767, Audio_DecodeS24ToS16(0x007FFFFFUL));
}

static void test_decode_24bit_min_negative(void)
{
    TEST_ASSERT_EQUAL_INT16(-32768, Audio_DecodeS24ToS16(0x00800000UL));
}

static void test_decode_24bit_negative_one_fullscale_step(void)
{
    TEST_ASSERT_EQUAL_INT16(-1, Audio_DecodeS24ToS16(0x00FFFFFFUL));
}

static void test_gain_unity_preserves_positive(void)
{
    TEST_ASSERT_EQUAL_INT16(12345, Audio_ApplyGainQ15(12345, 32768));
}

static void test_gain_half_scales_negative(void)
{
    TEST_ASSERT_EQUAL_INT16(-6000, Audio_ApplyGainQ15(-12000, 16384));
}

static void test_gain_saturates_positive(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, Audio_ApplyGainQ15(20000, 65536));
}

static void test_gain_saturates_negative(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, Audio_ApplyGainQ15(-20000, 65536));
}

static void test_negative_gain_is_safely_rejected(void)
{
    TEST_ASSERT_EQUAL_INT16(0, Audio_ApplyGainQ15(1234, -1));
}

static void test_mono_mix_cancels_opposite_channels(void)
{
    TEST_ASSERT_EQUAL_INT16(0, Audio_MixStereoToMono(20000, -20000));
}

static void test_mono_mix_handles_fullscale_without_overflow(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, Audio_MixStereoToMono(INT16_MAX, INT16_MAX));
}

static void test_mute_forces_zero(void)
{
    TEST_ASSERT_EQUAL_INT16(0, Audio_ApplyMute(23456, true));
}

static void test_unmuted_path_preserves_sample(void)
{
    TEST_ASSERT_EQUAL_INT16(-23456, Audio_ApplyMute(-23456, false));
}

static void test_block_processing_zero_length_accepts_null(void)
{
    TEST_ASSERT_TRUE(Audio_ProcessStereoToMono(NULL, NULL, 0U, 32768, false));
}

static void test_block_processing_rejects_null_input(void)
{
    int16_t mono[1] = {0};
    TEST_ASSERT_FALSE(Audio_ProcessStereoToMono(NULL, mono, 1U, 32768, false));
}

static void test_block_processing_rejects_null_output(void)
{
    const int16_t stereo[2] = {1, 1};
    TEST_ASSERT_FALSE(Audio_ProcessStereoToMono(stereo, NULL, 1U, 32768, false));
}

static void test_block_processing_rejects_negative_gain(void)
{
    const int16_t stereo[2] = {1, 1};
    int16_t mono[1] = {0};
    TEST_ASSERT_FALSE(Audio_ProcessStereoToMono(stereo, mono, 1U, -1, false));
}

static void test_block_processing_folds_and_gains_frames(void)
{
    const int16_t stereo[6] = {1000, 3000, -2000, 2000, 10000, 10000};
    int16_t mono[3] = {0};
    TEST_ASSERT_TRUE(Audio_ProcessStereoToMono(stereo, mono, 3U, 32768, false));
    TEST_ASSERT_EQUAL_INT16(2000, mono[0]);
    TEST_ASSERT_EQUAL_INT16(0, mono[1]);
    TEST_ASSERT_EQUAL_INT16(10000, mono[2]);
}

static void test_block_processing_mutes_every_frame(void)
{
    const int16_t stereo[4] = {1000, 1000, -1000, -1000};
    int16_t mono[2] = {123, 456};
    TEST_ASSERT_TRUE(Audio_ProcessStereoToMono(stereo, mono, 2U, 32768, true));
    TEST_ASSERT_EQUAL_INT16(0, mono[0]);
    TEST_ASSERT_EQUAL_INT16(0, mono[1]);
}

void RunAudioProcessingTests(void)
{
    RUN_TEST(test_sample_rate_accepts_16k);
    RUN_TEST(test_sample_rate_accepts_48k);
    RUN_TEST(test_sample_rate_rejects_44k1);
    RUN_TEST(test_decode_24bit_zero);
    RUN_TEST(test_decode_24bit_max_positive);
    RUN_TEST(test_decode_24bit_min_negative);
    RUN_TEST(test_decode_24bit_negative_one_fullscale_step);
    RUN_TEST(test_gain_unity_preserves_positive);
    RUN_TEST(test_gain_half_scales_negative);
    RUN_TEST(test_gain_saturates_positive);
    RUN_TEST(test_gain_saturates_negative);
    RUN_TEST(test_negative_gain_is_safely_rejected);
    RUN_TEST(test_mono_mix_cancels_opposite_channels);
    RUN_TEST(test_mono_mix_handles_fullscale_without_overflow);
    RUN_TEST(test_mute_forces_zero);
    RUN_TEST(test_unmuted_path_preserves_sample);
    RUN_TEST(test_block_processing_zero_length_accepts_null);
    RUN_TEST(test_block_processing_rejects_null_input);
    RUN_TEST(test_block_processing_rejects_null_output);
    RUN_TEST(test_block_processing_rejects_negative_gain);
    RUN_TEST(test_block_processing_folds_and_gains_frames);
    RUN_TEST(test_block_processing_mutes_every_frame);
}
