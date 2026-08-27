#include "connectivity_service.h"
#include "requirements.h"
#include "unity.h"

#include <limits.h>
#include <string.h>

static ConnectivityLinkState_t link_state(uint32_t now_ms,
                                           bool ready,
                                           bool paired,
                                           bool bt_connected)
{
    ConnectivityLinkState_t state;
    state.now_ms = now_ms;
    state.interfaces_ready = ready;
    state.paired_device_available = paired;
    state.bluetooth_connected = bt_connected;
    return state;
}

static void fill_strong_telecoil(int16_t *buffer, size_t count)
{
    for (size_t i = 0U; i < count; ++i) {
        buffer[i] = ((i & 1U) == 0U) ? 2000 : -2000;
    }
}

static void test_service_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_Init(NULL, 48000U, 0U));
}

static void test_service_init_accepts_16k(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 16000U, 0U));
    TEST_ASSERT_EQUAL_UINT32(16000U, service.sample_rate_hz);
}

static void test_service_init_accepts_48k(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_UINT32(48000U, service.sample_rate_hz);
}

static void test_service_init_rejects_44k1(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_SAMPLE_RATE,
                          ConnectivityService_Init(&service, 44100U, 0U));
}

static void test_service_step_rejects_null_arguments(void)
{
    ConnectivityService_t service;
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_Step(NULL, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_Step(&service, NULL));
}

static void test_service_ready_without_source_enters_idle(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(10U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, service.last_outputs.mode);
    TEST_ASSERT_TRUE(service.last_outputs.muted);
}

static void test_service_bluetooth_priority_is_exposed_in_status(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(10U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, service.last_outputs.active_source);
}

static void test_service_continuously_monitors_telecoil_from_idle(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, service.last_outputs.mode);

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, service.last_outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 3U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, service.last_outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, service.last_outputs.active_source);
}

static void test_service_inactive_telecoil_output_is_zero(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U));
    for (size_t i = 0U; i < 32U; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, output[i]);
    }
}

static void test_service_active_telecoil_produces_audio(void)
{
    ConnectivityService_t service;
    int16_t input[64];
    int16_t output[64];
    fill_strong_telecoil(input, 64U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 64U, 2U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 64U, 3U));
    bool any_nonzero = false;
    for (size_t i = 0U; i < 64U; ++i) {
        if (output[i] != 0) {
            any_nonzero = true;
        }
    }
    TEST_ASSERT_TRUE(any_nonzero);
}

static void test_service_bt_frame_at_100ms_is_accepted(void)
{
    ConnectivityService_t service;
    const int16_t input[4] = {100, -200, 300, -400};
    int16_t output[4] = {0};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(10U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessBluetoothBlock(&service, input, output, 4U, 10U,
                                                                    10U + REQ_BT_AUDIO_LATENCY_MAX_MS));
    TEST_ASSERT_EQUAL_INT16(100, output[0]);
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.bt_frames_processed);
}

static void test_service_bt_frame_over_100ms_is_rejected_and_zeroed(void)
{
    ConnectivityService_t service;
    const int16_t input[4] = {100, -200, 300, -400};
    int16_t output[4] = {1, 1, 1, 1};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(10U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_STALE_BT_FRAME,
                          ConnectivityService_ProcessBluetoothBlock(&service, input, output, 4U, 10U,
                                                                    11U + REQ_BT_AUDIO_LATENCY_MAX_MS));
    for (size_t i = 0U; i < 4U; ++i) {
        TEST_ASSERT_EQUAL_INT16(0, output[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.stale_bt_frames_dropped);
}

static void test_service_bt_processing_respects_gain(void)
{
    ConnectivityService_t service;
    const int16_t input[2] = {1000, -1000};
    int16_t output[2] = {0};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_SetGains(&service, 16384, 32768));
    ConnectivityLinkState_t state = link_state(10U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessBluetoothBlock(&service, input, output, 2U, 10U, 10U));
    TEST_ASSERT_EQUAL_INT16(500, output[0]);
    TEST_ASSERT_EQUAL_INT16(-500, output[1]);
}

static void test_service_negative_gain_is_rejected(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_SetGains(&service, -1, 32768));
}

static void test_service_disable_bluetooth_switches_to_valid_telecoil(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 3U));
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, service.last_outputs.active_source);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH,
                                                           4U));
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, service.last_outputs.active_source);
}

static void test_service_invalid_command_is_rejected(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_ApplyCommand(&service, (ConnectivityCommand_t)99, 1U));
}

static void test_service_fault_mutes_bluetooth_output_immediately(void)
{
    ConnectivityService_t service;
    const int16_t input[2] = {1000, 1000};
    int16_t output[2] = {1, 1};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    state = link_state(2U, true, true, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, service.last_outputs.mode);
    TEST_ASSERT_TRUE(service.last_outputs.muted);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessBluetoothBlock(&service, input, output, 2U, 2U, 2U));
    TEST_ASSERT_EQUAL_INT16(0, output[0]);
    TEST_ASSERT_EQUAL_INT16(0, output[1]);
}

static void test_service_counts_fault_and_recovery(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    state = link_state(2U, true, true, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.faults_entered);
    state = link_state(3U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    state.now_ms = 103U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.recoveries);
}

static void test_service_noise_floor_classifies_20db_boundary(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_SetTelecoilNoiseFloor(&service, 200U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_MEETS_20DB, service.telecoil_quality);
}

static void test_service_unset_noise_floor_reports_unknown(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U));
    TEST_ASSERT_EQUAL_INT(SIGNAL_QUALITY_UNKNOWN, service.telecoil_quality);
}

static void test_service_reset_diagnostics_preserves_state(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_GREATER_THAN(0U, service.diagnostics.controller_updates);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ResetDiagnostics(&service));
    TEST_ASSERT_EQUAL_UINT32(0U, service.diagnostics.controller_updates);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, service.last_outputs.active_source);
}

static void test_service_get_status_copies_latest_outputs(void)
{
    ConnectivityService_t service;
    ConnectivityOutputs_t outputs;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_GetStatus(&service, &outputs));
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
}

static void test_service_rejects_time_going_backwards(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 100U));
    ConnectivityLinkState_t state = link_state(90U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_Step(&service, &state));
}

static void test_service_accepts_millisecond_wrap_for_bt_audio_age(void)
{
    ConnectivityService_t service;
    const uint32_t start = UINT32_MAX - 20U;
    const int16_t input[1] = {1000};
    int16_t output[1] = {0};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, start));
    ConnectivityLinkState_t state = link_state(UINT32_MAX - 10U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ProcessBluetoothBlock(&service, input, output, 1U,
                                                                    UINT32_MAX - 5U, 20U));
    TEST_ASSERT_EQUAL_INT16(1000, output[0]);
}

static void test_service_process_functions_validate_zero_length(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_ProcessTelecoilBlock(&service, NULL, NULL, 0U, 1U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_ProcessBluetoothBlock(&service, NULL, NULL, 0U, 0U, 1U));
}


static void test_service_all_control_commands_are_supported(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_DISABLE_BOTH,
                                                           1U));
    TEST_ASSERT_FALSE(service.controller.bluetooth_enabled);
    TEST_ASSERT_FALSE(service.controller.telecoil_enabled);

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_ENABLE_BLUETOOTH,
                                                           2U));
    TEST_ASSERT_TRUE(service.controller.bluetooth_enabled);
    TEST_ASSERT_FALSE(service.controller.telecoil_enabled);

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_ENABLE_TELECOIL,
                                                           3U));
    TEST_ASSERT_TRUE(service.controller.telecoil_enabled);

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_DISABLE_TELECOIL,
                                                           4U));
    TEST_ASSERT_FALSE(service.controller.telecoil_enabled);

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_ENABLE_BOTH,
                                                           5U));
    TEST_ASSERT_TRUE(service.controller.bluetooth_enabled);
    TEST_ASSERT_TRUE(service.controller.telecoil_enabled);
}

static void test_service_public_apis_reject_uninitialised_state(void)
{
    ConnectivityService_t service;
    memset(&service, 0, sizeof(service));
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    ConnectivityOutputs_t outputs;
    int16_t input[1] = {0};
    int16_t output[1] = {123};

    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_ENABLE_BOTH,
                                                           1U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_SetGains(&service, 32768, 32768));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_SetTelecoilNoiseFloor(&service, 10U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessTelecoilBlock(&service,
                                                                   input,
                                                                   output,
                                                                   1U,
                                                                   1U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessBluetoothBlock(&service,
                                                                    input,
                                                                    output,
                                                                    1U,
                                                                    0U,
                                                                    1U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_GetStatus(&service, &outputs));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ResetDiagnostics(&service));
}

static void test_service_public_apis_reject_null_service(void)
{
    ConnectivityOutputs_t outputs;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_ApplyCommand(NULL,
                                                           CONNECTIVITY_COMMAND_ENABLE_BOTH,
                                                           0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_SetGains(NULL, 32768, 32768));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_SetTelecoilNoiseFloor(NULL, 10U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_GetStatus(NULL, &outputs));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_ResetDiagnostics(NULL));
}

static void test_service_status_rejects_null_output(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_ARGUMENT,
                          ConnectivityService_GetStatus(&service, NULL));
}

static void test_service_time_validation_applies_to_commands_and_audio(void)
{
    ConnectivityService_t service;
    const int16_t input[1] = {100};
    int16_t output[1] = {77};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 100U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_ENABLE_BOTH,
                                                           90U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessTelecoilBlock(&service,
                                                                   input,
                                                                   output,
                                                                   1U,
                                                                   90U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessBluetoothBlock(&service,
                                                                    input,
                                                                    output,
                                                                    1U,
                                                                    80U,
                                                                    90U));
    TEST_ASSERT_EQUAL_INT16(0, output[0]);
}

static void test_service_rejects_bt_receive_timestamp_in_future(void)
{
    ConnectivityService_t service;
    const int16_t input[1] = {100};
    int16_t output[1] = {77};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessBluetoothBlock(&service,
                                                                    input,
                                                                    output,
                                                                    1U,
                                                                    10U,
                                                                    5U));
    TEST_ASSERT_EQUAL_INT16(0, output[0]);
}

static void test_service_controller_error_is_propagated(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    service.controller.mode = (ConnectivityMode_t)99;
    ConnectivityLinkState_t state = link_state(1U, true, false, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_Step(&service, &state));
}

static void test_service_connect_and_reconnect_events_are_counted(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    ConnectivityLinkState_t state = link_state(1U, true, true, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.bt_connect_requests);

    state = link_state(10U, true, true, true);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    state = link_state(11U, true, true, false);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    state.now_ms = 2011U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Step(&service, &state));
    TEST_ASSERT_EQUAL_UINT32(1U, service.diagnostics.bt_reconnect_requests);
}


static void test_service_command_propagates_controller_time_error(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    service.controller.last_update_ms = 100U;
    service.link_state.now_ms = 0U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_ENABLE_BOTH,
                                                           1U));
}

static void test_service_telecoil_processing_propagates_controller_error(void)
{
    ConnectivityService_t service;
    const int16_t input[1] = {1000};
    int16_t output[1] = {77};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    service.controller.mode = (ConnectivityMode_t)99;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessTelecoilBlock(&service,
                                                                   input,
                                                                   output,
                                                                   1U,
                                                                   1U));
    TEST_ASSERT_EQUAL_INT16(0, output[0]);
}

static void test_service_bluetooth_processing_propagates_controller_error(void)
{
    ConnectivityService_t service;
    const int16_t input[1] = {1000};
    int16_t output[1] = {77};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    service.controller.mode = (ConnectivityMode_t)99;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_ERR_STATE,
                          ConnectivityService_ProcessBluetoothBlock(&service,
                                                                    input,
                                                                    output,
                                                                    1U,
                                                                    0U,
                                                                    1U));
    TEST_ASSERT_EQUAL_INT16(0, output[0]);
}


static void test_service_bluetooth_only_command_is_atomic(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_BLUETOOTH_ONLY,
                                                           1U));
    TEST_ASSERT_TRUE(service.controller.bluetooth_enabled);
    TEST_ASSERT_FALSE(service.controller.telecoil_enabled);
}

static void test_service_telecoil_only_command_is_atomic(void)
{
    ConnectivityService_t service;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_Init(&service, 48000U, 0U));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_SERVICE_OK,
                          ConnectivityService_ApplyCommand(&service,
                                                           CONNECTIVITY_COMMAND_TELECOIL_ONLY,
                                                           1U));
    TEST_ASSERT_FALSE(service.controller.bluetooth_enabled);
    TEST_ASSERT_TRUE(service.controller.telecoil_enabled);
}

void RunConnectivityServiceTests(void)
{
    RUN_TEST(test_service_init_rejects_null);
    RUN_TEST(test_service_init_accepts_16k);
    RUN_TEST(test_service_init_accepts_48k);
    RUN_TEST(test_service_init_rejects_44k1);
    RUN_TEST(test_service_step_rejects_null_arguments);
    RUN_TEST(test_service_ready_without_source_enters_idle);
    RUN_TEST(test_service_bluetooth_priority_is_exposed_in_status);
    RUN_TEST(test_service_continuously_monitors_telecoil_from_idle);
    RUN_TEST(test_service_inactive_telecoil_output_is_zero);
    RUN_TEST(test_service_active_telecoil_produces_audio);
    RUN_TEST(test_service_bt_frame_at_100ms_is_accepted);
    RUN_TEST(test_service_bt_frame_over_100ms_is_rejected_and_zeroed);
    RUN_TEST(test_service_bt_processing_respects_gain);
    RUN_TEST(test_service_negative_gain_is_rejected);
    RUN_TEST(test_service_disable_bluetooth_switches_to_valid_telecoil);
    RUN_TEST(test_service_invalid_command_is_rejected);
    RUN_TEST(test_service_fault_mutes_bluetooth_output_immediately);
    RUN_TEST(test_service_counts_fault_and_recovery);
    RUN_TEST(test_service_noise_floor_classifies_20db_boundary);
    RUN_TEST(test_service_unset_noise_floor_reports_unknown);
    RUN_TEST(test_service_reset_diagnostics_preserves_state);
    RUN_TEST(test_service_get_status_copies_latest_outputs);
    RUN_TEST(test_service_rejects_time_going_backwards);
    RUN_TEST(test_service_accepts_millisecond_wrap_for_bt_audio_age);
    RUN_TEST(test_service_process_functions_validate_zero_length);
    RUN_TEST(test_service_all_control_commands_are_supported);
    RUN_TEST(test_service_bluetooth_only_command_is_atomic);
    RUN_TEST(test_service_telecoil_only_command_is_atomic);
    RUN_TEST(test_service_public_apis_reject_uninitialised_state);
    RUN_TEST(test_service_public_apis_reject_null_service);
    RUN_TEST(test_service_status_rejects_null_output);
    RUN_TEST(test_service_time_validation_applies_to_commands_and_audio);
    RUN_TEST(test_service_rejects_bt_receive_timestamp_in_future);
    RUN_TEST(test_service_controller_error_is_propagated);
    RUN_TEST(test_service_connect_and_reconnect_events_are_counted);
    RUN_TEST(test_service_command_propagates_controller_time_error);
    RUN_TEST(test_service_telecoil_processing_propagates_controller_error);
    RUN_TEST(test_service_bluetooth_processing_propagates_controller_error);
}
