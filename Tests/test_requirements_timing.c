#include "connectivity_service.h"
#include "requirements.h"
#include "unity.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

static ConnectivityLinkState_t link_state(uint32_t now_ms, bool ready, bool paired, bool connected)
{
    ConnectivityLinkState_t state = {now_ms, ready, paired, connected};
    return state;
}

static void fill_strong(int16_t *samples, size_t count)
{
    for (size_t i = 0U; i < count; ++i) {
        samples[i] = ((i & 1U) == 0U) ? 2000 : -2000;
    }
}

static void fill_silence(int16_t *samples, size_t count)
{
    for (size_t i = 0U; i < count; ++i) {
        samples[i] = 0;
    }
}

static void activate_telecoil(ConnectivityService_t *service, uint32_t start_ms)
{
    int16_t in[16];
    int16_t out[16];
    fill_strong(in, 16U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(service, in, out, 16U, start_ms));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(service, in, out, 16U, start_ms + 1U));
    TEST_ASSERT_TRUE(service->telecoil_valid_timed);
}

static void test_timing_contract_worst_case_is_inside_100ms(void)
{
    TEST_ASSERT_LESS_OR_EQUAL(REQ_SOURCE_SWITCH_MAX_MS, DESIGN_TELECOIL_FAULT_WORST_CASE_MS);
}

static void test_service_interval_at_40ms_is_not_a_miss(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(DESIGN_SERVICE_INTERVAL_MAX_MS, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(0U, service.diagnostics.service_deadline_misses);
}

static void test_service_interval_over_40ms_is_counted(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(DESIGN_SERVICE_INTERVAL_MAX_MS + 1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.service_deadline_misses);
}

static void test_max_service_interval_is_recorded(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(10U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    state.now_ms = 35U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(25U, service.diagnostics.max_service_interval_ms);
}

static void test_reset_diagnostics_rebases_service_timer(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(100U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_ResetDiagnostics(&service));
    state.now_ms = 120U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(0U, service.diagnostics.service_deadline_misses);
    TEST_ASSERT_EQUAL_UINT32(20U, service.diagnostics.max_service_interval_ms);
}

static void test_telecoil_loss_does_not_fault_before_confirmation(void)
{
    ConnectivityService_t service;
    int16_t in[16];
    int16_t out[16];
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    activate_telecoil(&service, 2U);
    fill_silence(in, 16U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, in, out, 16U, 10U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, service.last_outputs.mode);
}

static void test_telecoil_loss_faults_at_exact_confirmation(void)
{
    ConnectivityService_t service;
    int16_t in[16];
    int16_t out[16];
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    activate_telecoil(&service, 2U);
    fill_silence(in, 16U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, in, out, 16U, 10U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, in, out, 16U,
                                                                   10U + DESIGN_TELECOIL_LOSS_CONFIRM_MS));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, service.last_outputs.mode);
    TEST_ASSERT_TRUE(service.last_outputs.muted);
}

static void test_telecoil_transient_absence_cancels_timer(void)
{
    ConnectivityService_t service;
    int16_t strong[16];
    int16_t silent[16];
    int16_t out[16];
    fill_strong(strong, 16U);
    fill_silence(silent, 16U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    activate_telecoil(&service, 2U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, silent, out, 16U, 10U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, strong, out, 16U, 20U));
    TEST_ASSERT_FALSE(service.telecoil_absence_tracking);
    TEST_ASSERT_TRUE(service.telecoil_valid_timed);
}

static void test_time_qualified_transition_counter_uses_service_validity(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    activate_telecoil(&service, 2U);
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.telecoil_valid_transitions);
}

static void test_timed_telecoil_validity_survives_detector_block_deactivation_until_timer(void)
{
    ConnectivityService_t service;
    int16_t silent[16];
    int16_t out[16];
    fill_silence(silent, 16U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    activate_telecoil(&service, 2U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, silent, out, 16U, 10U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, silent, out, 16U, 11U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, silent, out, 16U, 12U));
    TEST_ASSERT_FALSE(service.telecoil_detector.valid_signal);
    TEST_ASSERT_TRUE(service.telecoil_valid_timed);
}

static void test_timing_diagnostics_are_wrap_safe(void)
{
    ConnectivityService_t service;
    const uint32_t start = UINT32_MAX - 10U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, start));
    ConnectivityLinkState_t state = link_state(5U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(16U, service.diagnostics.max_service_interval_ms);
}

static void test_command_updates_timing_diagnostics(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service, CONNECTIVITY_COMMAND_ENABLE_BOTH, 5U));
    TEST_ASSERT_EQUAL_UINT32(5U, service.diagnostics.max_service_interval_ms);
}

static void test_bluetooth_processing_updates_timing_diagnostics(void)
{
    ConnectivityService_t service;
    const int16_t in[1] = {1};
    int16_t out[1] = {0};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessBluetoothBlock(&service, in, out, 1U, 1U, 20U));
    TEST_ASSERT_EQUAL_UINT32(19U, service.diagnostics.max_service_interval_ms);
}

static void test_telecoil_processing_updates_timing_diagnostics(void)
{
    ConnectivityService_t service;
    int16_t in[16];
    int16_t out[16];
    fill_strong(in, 16U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK, ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, in, out, 16U, 20U));
    TEST_ASSERT_EQUAL_UINT32(20U, service.diagnostics.max_service_interval_ms);
}

void RunRequirementsTimingTests(void)
{
    RUN_TEST(test_timing_contract_worst_case_is_inside_100ms);
    RUN_TEST(test_service_interval_at_40ms_is_not_a_miss);
    RUN_TEST(test_service_interval_over_40ms_is_counted);
    RUN_TEST(test_max_service_interval_is_recorded);
    RUN_TEST(test_reset_diagnostics_rebases_service_timer);
    RUN_TEST(test_telecoil_loss_does_not_fault_before_confirmation);
    RUN_TEST(test_telecoil_loss_faults_at_exact_confirmation);
    RUN_TEST(test_telecoil_transient_absence_cancels_timer);
    RUN_TEST(test_time_qualified_transition_counter_uses_service_validity);
    RUN_TEST(test_timed_telecoil_validity_survives_detector_block_deactivation_until_timer);
    RUN_TEST(test_timing_diagnostics_are_wrap_safe);
    RUN_TEST(test_command_updates_timing_diagnostics);
    RUN_TEST(test_bluetooth_processing_updates_timing_diagnostics);
    RUN_TEST(test_telecoil_processing_updates_timing_diagnostics);
}
