#include "signal_quality.h"
#include "unity.h"

#include <limits.h>

static void test_quality_rejects_null_classification(void)
{
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_ERR_ARGUMENT,
                          SignalQuality_Classify20dB(1000U, 100U, NULL));
}

static void test_quality_exact_20db_boundary_passes(void)
{
    SignalQualityClass_t classification = SIGNAL_QUALITY_UNKNOWN;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_Classify20dB(1000U, 100U, &classification));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_MEETS_20DB, classification);
}

static void test_quality_just_below_20db_fails(void)
{
    SignalQualityClass_t classification = SIGNAL_QUALITY_UNKNOWN;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_Classify20dB(999U, 100U, &classification));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_BELOW_20DB, classification);
}

static void test_quality_zero_signal_and_noise_is_unknown(void)
{
    SignalQualityClass_t classification = SIGNAL_QUALITY_MEETS_20DB;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_Classify20dB(0U, 0U, &classification));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_UNKNOWN, classification);
}

static void test_quality_nonzero_signal_with_zero_noise_meets_requirement(void)
{
    SignalQualityClass_t classification = SIGNAL_QUALITY_UNKNOWN;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_Classify20dB(1U, 0U, &classification));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_MEETS_20DB, classification);
}

static void test_quality_uses_wide_intermediate_without_overflow(void)
{
    SignalQualityClass_t classification = SIGNAL_QUALITY_UNKNOWN;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_Classify20dB(UINT32_MAX, 400000000U, &classification));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_MEETS_20DB, classification);
}

static void test_ratio_rejects_null_output(void)
{
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_ERR_ARGUMENT,
                          SignalQuality_AmplitudeRatioQ8(10U, 1U, NULL));
}

static void test_ratio_q8_unity_is_256(void)
{
    uint32_t ratio = 0U;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_AmplitudeRatioQ8(100U, 100U, &ratio));
    TEST_ASSERT_EQUAL_UINT32(256U, ratio);
}

static void test_ratio_q8_ten_to_one_is_2560(void)
{
    uint32_t ratio = 0U;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_AmplitudeRatioQ8(1000U, 100U, &ratio));
    TEST_ASSERT_EQUAL_UINT32(2560U, ratio);
}

static void test_ratio_zero_over_zero_returns_zero(void)
{
    uint32_t ratio = 99U;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_AmplitudeRatioQ8(0U, 0U, &ratio));
    TEST_ASSERT_EQUAL_UINT32(0U, ratio);
}

static void test_ratio_nonzero_over_zero_saturates(void)
{
    uint32_t ratio = 0U;
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_OK,
                          SignalQuality_AmplitudeRatioQ8(1U, 0U, &ratio));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, ratio);
}

void RunSignalQualityTests(void)
{
    RUN_TEST(test_quality_rejects_null_classification);
    RUN_TEST(test_quality_exact_20db_boundary_passes);
    RUN_TEST(test_quality_just_below_20db_fails);
    RUN_TEST(test_quality_zero_signal_and_noise_is_unknown);
    RUN_TEST(test_quality_nonzero_signal_with_zero_noise_meets_requirement);
    RUN_TEST(test_quality_uses_wide_intermediate_without_overflow);
    RUN_TEST(test_ratio_rejects_null_output);
    RUN_TEST(test_ratio_q8_unity_is_256);
    RUN_TEST(test_ratio_q8_ten_to_one_is_2560);
    RUN_TEST(test_ratio_zero_over_zero_returns_zero);
    RUN_TEST(test_ratio_nonzero_over_zero_saturates);
}
