#include "audio_dynamics.h"
#include "unity.h"

#include <limits.h>
#include <stdint.h>

static AudioDynamics_t make_state(void)
{
    AudioDynamics_t state = {0};
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    (void)AudioDynamics_Init(&state, &config);
    return state;
}

static void test_dynamics_default_config_is_valid(void)
{
    AudioDynamics_t state;
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_OK, AudioDynamics_Init(&state, &config));
}

static void test_dynamics_init_rejects_null_state(void)
{
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_ARGUMENT, AudioDynamics_Init(NULL, &config));
}

static void test_dynamics_init_rejects_null_config(void)
{
    AudioDynamics_t state;
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_CONFIG, AudioDynamics_Init(&state, NULL));
}

static void test_dynamics_init_rejects_bad_alpha(void)
{
    AudioDynamics_t state;
    AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    config.dc_alpha_q15 = 40000;
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_CONFIG, AudioDynamics_Init(&state, &config));
}

static void test_dynamics_init_rejects_bad_gain_range(void)
{
    AudioDynamics_t state;
    AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    config.min_gain_q15 = 40000;
    config.max_gain_q15 = 30000;
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_CONFIG, AudioDynamics_Init(&state, &config));
}

static void test_dynamics_reset_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_ARGUMENT, AudioDynamics_Reset(NULL));
}

static void test_dynamics_reset_rejects_uninitialised(void)
{
    AudioDynamics_t state = {0};
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_STATE, AudioDynamics_Reset(&state));
}

static void test_dynamics_reset_restores_unity_gain(void)
{
    AudioDynamics_t state = make_state();
    state.current_gain_q15 = 12345;
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_OK, AudioDynamics_Reset(&state));
    TEST_ASSERT_EQUAL_INT(32768, state.current_gain_q15);
}

static void test_limiter_zero_limit_returns_zero(void)
{
    TEST_ASSERT_EQUAL_INT16(0, AudioDynamics_LimitS16(1000, 0));
}

static void test_limiter_clamps_positive(void)
{
    TEST_ASSERT_EQUAL_INT16(10000, AudioDynamics_LimitS16(20000, 10000));
}

static void test_limiter_clamps_negative(void)
{
    TEST_ASSERT_EQUAL_INT16(-10000, AudioDynamics_LimitS16(-20000, 10000));
}

static void test_limiter_preserves_in_range(void)
{
    TEST_ASSERT_EQUAL_INT16(-1234, AudioDynamics_LimitS16(-1234, 10000));
    TEST_ASSERT_EQUAL_INT16(1234, AudioDynamics_LimitS16(1234, 50000));
}

static void test_target_gain_silence_uses_maximum(void)
{
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(config.max_gain_q15, AudioDynamics_TargetGainQ15(&config, 0U));
}

static void test_target_gain_at_target_is_unity(void)
{
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(32768,
                          AudioDynamics_TargetGainQ15(&config, (uint32_t)config.target_peak));
}

static void test_target_gain_loud_signal_reduces_gain(void)
{
    AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    TEST_ASSERT_LESS_THAN(32768,
                          AudioDynamics_TargetGainQ15(&config, (uint32_t)INT16_MAX));
    config.target_peak = 1000;
    TEST_ASSERT_EQUAL_INT(config.min_gain_q15,
                          AudioDynamics_TargetGainQ15(&config, (uint32_t)INT16_MAX));
}

static void test_target_gain_invalid_config_returns_zero(void)
{
    TEST_ASSERT_EQUAL_INT(0, AudioDynamics_TargetGainQ15(NULL, 1000U));
}

static void test_process_rejects_uninitialised_state(void)
{
    AudioDynamics_t state = {0};
    const int16_t input[1] = {1};
    int16_t output[1] = {0};
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_ARGUMENT,
                          AudioDynamics_ProcessBlock(NULL, input, output, 1U, false));
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_STATE,
                          AudioDynamics_ProcessBlock(&state, input, output, 1U, false));
}

static void test_process_zero_length_allows_null_buffers(void)
{
    AudioDynamics_t state = make_state();
    const int16_t input[1] = {1};
    int16_t output[1] = {0};
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_OK,
                          AudioDynamics_ProcessBlock(&state, NULL, NULL, 0U, false));
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_ARGUMENT,
                          AudioDynamics_ProcessBlock(&state, NULL, output, 1U, false));
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_ERR_ARGUMENT,
                          AudioDynamics_ProcessBlock(&state, input, NULL, 1U, false));
}

static void test_process_mute_zeroes_block(void)
{
    AudioDynamics_t state = make_state();
    state.config.release_shift = 0U;
    const int16_t input[3] = {1000, -2000, 3000};
    int16_t output[3] = {1, 1, 1};
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_OK,
                          AudioDynamics_ProcessBlock(&state, input, output, 3U, true));
    TEST_ASSERT_EQUAL_INT16(0, output[0]);
    TEST_ASSERT_EQUAL_INT16(0, output[1]);
    TEST_ASSERT_EQUAL_INT16(0, output[2]);
}

static void test_process_dc_rejection_reduces_steady_state(void)
{
    AudioDynamics_t state = make_state();
    int16_t input[64];
    int16_t output[64];
    for (size_t i = 0U; i < 64U; ++i) {
        input[i] = 1000;
    }
    TEST_ASSERT_EQUAL_INT(AUDIO_DYNAMICS_OK,
                          AudioDynamics_ProcessBlock(&state, input, output, 64U, false));
    TEST_ASSERT_LESS_THAN(1000, (output[63] < 0) ? -output[63] : output[63]);
}

void RunAudioDynamicsTests(void)
{
    RUN_TEST(test_dynamics_default_config_is_valid);
    RUN_TEST(test_dynamics_init_rejects_null_state);
    RUN_TEST(test_dynamics_init_rejects_null_config);
    RUN_TEST(test_dynamics_init_rejects_bad_alpha);
    RUN_TEST(test_dynamics_init_rejects_bad_gain_range);
    RUN_TEST(test_dynamics_reset_rejects_null);
    RUN_TEST(test_dynamics_reset_rejects_uninitialised);
    RUN_TEST(test_dynamics_reset_restores_unity_gain);
    RUN_TEST(test_limiter_zero_limit_returns_zero);
    RUN_TEST(test_limiter_clamps_positive);
    RUN_TEST(test_limiter_clamps_negative);
    RUN_TEST(test_limiter_preserves_in_range);
    RUN_TEST(test_target_gain_silence_uses_maximum);
    RUN_TEST(test_target_gain_at_target_is_unity);
    RUN_TEST(test_target_gain_loud_signal_reduces_gain);
    RUN_TEST(test_target_gain_invalid_config_returns_zero);
    RUN_TEST(test_process_rejects_uninitialised_state);
    RUN_TEST(test_process_zero_length_allows_null_buffers);
    RUN_TEST(test_process_mute_zeroes_block);
    RUN_TEST(test_process_dc_rejection_reduces_steady_state);
}
