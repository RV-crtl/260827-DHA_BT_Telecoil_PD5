#include "telecoil_detector.h"
#include "unity.h"

#include <limits.h>
#include <string.h>

#define BLOCK_SAMPLES 32U

static void fill_square(int16_t *samples, int16_t amplitude)
{
    for (size_t i = 0U; i < BLOCK_SAMPLES; ++i) {
        samples[i] = ((i & 1U) == 0U) ? amplitude : (int16_t)-amplitude;
    }
}

static TelecoilDetector_t new_detector(void)
{
    TelecoilDetector_t detector;
    const TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    (void)TelecoilDetector_Init(&detector, &config);
    return detector;
}

static void test_default_config_has_hysteresis(void)
{
    const TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    TEST_ASSERT_GREATER_THAN(config.absent_rms_threshold, config.present_rms_threshold);
}

static void test_init_rejects_null_detector(void)
{
    const TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_ERR_ARGUMENT, TelecoilDetector_Init(NULL, &config));
}

static void test_init_rejects_inverted_thresholds(void)
{
    TelecoilDetector_t detector;
    TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    config.present_rms_threshold = 500U;
    config.absent_rms_threshold = 600U;
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_ERR_CONFIG, TelecoilDetector_Init(&detector, &config));
}

static void test_process_rejects_null_samples(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_ERR_ARGUMENT,
                          TelecoilDetector_Process(&detector, NULL, BLOCK_SAMPLES, &metrics));
}

static void test_process_rejects_zero_samples(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    const int16_t sample = 0;
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_ERR_ARGUMENT,
                          TelecoilDetector_Process(&detector, &sample, 0U, &metrics));
}

static void test_silence_is_not_valid_signal(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t samples[BLOCK_SAMPLES] = {0};
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_OK,
                          TelecoilDetector_Process(&detector, samples, BLOCK_SAMPLES, &metrics));
    TEST_ASSERT_FALSE(metrics.valid_signal);
    TEST_ASSERT_EQUAL_UINT32(0U, metrics.rms);
}

static void test_one_strong_block_does_not_false_trigger(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t samples[BLOCK_SAMPLES];
    fill_square(samples, 2000);
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_OK,
                          TelecoilDetector_Process(&detector, samples, BLOCK_SAMPLES, &metrics));
    TEST_ASSERT_TRUE(metrics.raw_candidate_present);
    TEST_ASSERT_FALSE(metrics.valid_signal);
}

static void test_two_consecutive_strong_blocks_activate(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t samples[BLOCK_SAMPLES];
    fill_square(samples, 2000);
    (void)TelecoilDetector_Process(&detector, samples, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_OK,
                          TelecoilDetector_Process(&detector, samples, BLOCK_SAMPLES, &metrics));
    TEST_ASSERT_TRUE(metrics.valid_signal);
    TEST_ASSERT_TRUE(metrics.state_changed);
}

static void test_middle_hysteresis_band_holds_active_state(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t strong[BLOCK_SAMPLES];
    int16_t middle[BLOCK_SAMPLES];
    fill_square(strong, 2000);
    fill_square(middle, 750);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_TRUE(detector.valid_signal);
    (void)TelecoilDetector_Process(&detector, middle, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_TRUE(metrics.valid_signal);
}

static void test_three_absent_blocks_deactivate(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t strong[BLOCK_SAMPLES];
    int16_t silence[BLOCK_SAMPLES] = {0};
    fill_square(strong, 2000);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, silence, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_TRUE(metrics.valid_signal);
    (void)TelecoilDetector_Process(&detector, silence, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_TRUE(metrics.valid_signal);
    (void)TelecoilDetector_Process(&detector, silence, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_FALSE(metrics.valid_signal);
    TEST_ASSERT_TRUE(metrics.state_changed);
}

static void test_transient_absence_resets_absent_streak(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t strong[BLOCK_SAMPLES];
    int16_t silence[BLOCK_SAMPLES] = {0};
    fill_square(strong, 2000);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, silence, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, silence, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, silence, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_TRUE(metrics.valid_signal);
}

static void test_heavily_clipped_block_is_rejected(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t clipped[BLOCK_SAMPLES];
    fill_square(clipped, INT16_MAX);
    (void)TelecoilDetector_Process(&detector, clipped, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_FALSE(metrics.raw_candidate_present);
    TEST_ASSERT_EQUAL_UINT32(BLOCK_SAMPLES, metrics.clipped_samples);
}

static void test_int16_min_is_handled_without_overflow(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t samples[BLOCK_SAMPLES];
    for (size_t i = 0U; i < BLOCK_SAMPLES; ++i) {
        samples[i] = ((i & 1U) == 0U) ? INT16_MIN : INT16_MAX;
    }
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_OK,
                          TelecoilDetector_Process(&detector, samples, BLOCK_SAMPLES, &metrics));
    TEST_ASSERT_GREATER_THAN(32000U, metrics.rms);
}

static void test_reset_clears_latched_state(void)
{
    TelecoilDetector_t detector = new_detector();
    TelecoilBlockMetrics_t metrics;
    int16_t strong[BLOCK_SAMPLES];
    fill_square(strong, 2000);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    (void)TelecoilDetector_Process(&detector, strong, BLOCK_SAMPLES, &metrics);
    TEST_ASSERT_TRUE(detector.valid_signal);
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_OK, TelecoilDetector_Reset(&detector));
    TEST_ASSERT_FALSE(detector.valid_signal);
    TEST_ASSERT_EQUAL_UINT(0U, detector.present_streak);
    TEST_ASSERT_EQUAL_UINT(0U, detector.absent_streak);
}


static void test_reset_rejects_null_detector(void)
{
    TEST_ASSERT_EQUAL_INT(TELECOIL_DETECTOR_ERR_ARGUMENT,
                          TelecoilDetector_Reset(NULL));
}

void RunTelecoilDetectorTests(void)
{
    RUN_TEST(test_default_config_has_hysteresis);
    RUN_TEST(test_init_rejects_null_detector);
    RUN_TEST(test_init_rejects_inverted_thresholds);
    RUN_TEST(test_process_rejects_null_samples);
    RUN_TEST(test_process_rejects_zero_samples);
    RUN_TEST(test_silence_is_not_valid_signal);
    RUN_TEST(test_one_strong_block_does_not_false_trigger);
    RUN_TEST(test_two_consecutive_strong_blocks_activate);
    RUN_TEST(test_middle_hysteresis_band_holds_active_state);
    RUN_TEST(test_three_absent_blocks_deactivate);
    RUN_TEST(test_transient_absence_resets_absent_streak);
    RUN_TEST(test_heavily_clipped_block_is_rejected);
    RUN_TEST(test_int16_min_is_handled_without_overflow);
    RUN_TEST(test_reset_clears_latched_state);
    RUN_TEST(test_reset_rejects_null_detector);
}
