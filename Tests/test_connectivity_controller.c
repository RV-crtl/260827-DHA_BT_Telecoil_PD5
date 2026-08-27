#include "connectivity_controller.h"
#include "unity.h"

#include <string.h>

static ConnectivityController_t new_controller(uint32_t start_ms)
{
    ConnectivityController_t controller;
    const ConnectivityConfig_t config = Connectivity_DefaultConfig();
    (void)Connectivity_Init(&controller, &config, start_ms);
    return controller;
}

static ConnectivityInputs_t input_at(uint32_t now_ms)
{
    ConnectivityInputs_t inputs;
    memset(&inputs, 0, sizeof(inputs));
    inputs.now_ms = now_ms;
    return inputs;
}

static ConnectivityOutputs_t update_ok(ConnectivityController_t *controller,
                                       ConnectivityInputs_t inputs)
{
    ConnectivityOutputs_t outputs;
    memset(&outputs, 0, sizeof(outputs));
    (void)Connectivity_Update(controller, &inputs, &outputs);
    return outputs;
}

static void activate_bluetooth(ConnectivityController_t *controller, uint32_t now_ms)
{
    ConnectivityInputs_t inputs = input_at(now_ms);
    inputs.interfaces_ready = true;
    inputs.bluetooth_connected = true;
    inputs.telecoil_valid = true;
    (void)update_ok(controller, inputs);
}

static void activate_telecoil(ConnectivityController_t *controller, uint32_t now_ms)
{
    ConnectivityInputs_t inputs = input_at(now_ms);
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    (void)update_ok(controller, inputs);
}

static void fault_bluetooth_link(ConnectivityController_t *controller, uint32_t now_ms)
{
    ConnectivityInputs_t inputs = input_at(now_ms);
    inputs.interfaces_ready = true;
    inputs.paired_device_available = true;
    inputs.bluetooth_connected = false;
    (void)update_ok(controller, inputs);
}

static void test_default_timing_matches_requirements(void)
{
    const ConnectivityConfig_t config = Connectivity_DefaultConfig();
    TEST_ASSERT_EQUAL_UINT32(3000U, config.startup_timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(5000U, config.bt_connect_timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(2000U, config.bt_reconnect_period_ms);
    TEST_ASSERT_EQUAL_UINT32(2000U, config.recovery_limit_ms);
    TEST_ASSERT_EQUAL_UINT32(5000U, config.fault_to_idle_ms);
    TEST_ASSERT_LESS_OR_EQUAL(config.recovery_limit_ms, config.recovery_stable_ms);
}

static void test_init_rejects_null_controller(void)
{
    const ConnectivityConfig_t config = Connectivity_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_ARGUMENT, Connectivity_Init(NULL, &config, 0U));
}

static void test_init_rejects_invalid_recovery_timing(void)
{
    ConnectivityController_t controller;
    ConnectivityConfig_t config = Connectivity_DefaultConfig();
    config.recovery_stable_ms = config.recovery_limit_ms + 1U;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_CONFIG, Connectivity_Init(&controller, &config, 0U));
}

static void test_initial_state_is_m1_muted_with_no_source(void)
{
    const ConnectivityController_t controller = new_controller(0U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M1_INITIALISING, controller.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_NONE, controller.active_source);
    TEST_ASSERT_TRUE(controller.muted);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_NONE, controller.fault);
}

static void test_startup_before_3s_remains_m1(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(2999U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M1_INITIALISING, outputs.mode);
}

static void test_startup_at_3s_without_interfaces_enters_fault(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(3000U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_STARTUP_TIMEOUT, outputs.fault);
    TEST_ASSERT_TRUE(outputs.muted);
}

static void test_ready_without_any_source_enters_idle(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(100U);
    inputs.interfaces_ready = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
    TEST_ASSERT_TRUE(outputs.muted);
}

static void test_bluetooth_source_activates_m2(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(100U);
    inputs.interfaces_ready = true;
    inputs.bluetooth_connected = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
    TEST_ASSERT_FALSE(outputs.muted);
}

static void test_telecoil_source_activates_m2(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(100U);
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
}

static void test_bluetooth_has_priority_when_both_sources_valid(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(100U);
    inputs.interfaces_ready = true;
    inputs.bluetooth_connected = true;
    inputs.telecoil_valid = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
}

static void test_paired_device_requests_initial_connection(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(1000U);
    inputs.interfaces_ready = true;
    inputs.paired_device_available = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_TRUE(outputs.request_bt_connect);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M1_INITIALISING, outputs.mode);
}

static void test_connection_just_before_5s_has_not_timed_out(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(4999U);
    inputs.interfaces_ready = true;
    inputs.paired_device_available = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_NONE, outputs.fault);
}

static void test_connection_at_5s_without_link_enters_fault(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(5000U);
    inputs.interfaces_ready = true;
    inputs.paired_device_available = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_BT_CONNECTION_TIMEOUT, outputs.fault);
}

static void test_telecoil_switches_to_bluetooth_immediately_when_bt_arrives(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_telecoil(&controller, 100U);
    ConnectivityInputs_t inputs = input_at(150U);
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    inputs.bluetooth_connected = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
    TEST_ASSERT_EQUAL_UINT32(150U, outputs.source_changed_ms);
}

static void test_bluetooth_link_loss_enters_m4(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityOutputs_t outputs;
    Connectivity_GetOutputs(&controller, &outputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_BT_LINK_LOSS, outputs.fault);
}

static void test_bluetooth_link_loss_mutes_immediately(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    ConnectivityInputs_t inputs = input_at(200U);
    inputs.interfaces_ready = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_TRUE(outputs.muted);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_NONE, outputs.active_source);
    TEST_ASSERT_EQUAL_UINT32(200U, outputs.mode_changed_ms);
}

static void test_invalid_active_telecoil_enters_m4(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_telecoil(&controller, 100U);
    ConnectivityInputs_t inputs = input_at(200U);
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = false;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_TELECOIL_INVALID, outputs.fault);
}

static void test_reconnect_not_requested_before_2s(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(2199U);
    inputs.paired_device_available = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_FALSE(outputs.request_bt_reconnect);
}

static void test_reconnect_requested_exactly_at_2s(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(2200U);
    inputs.paired_device_available = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_TRUE(outputs.request_bt_reconnect);
}

static void test_reconnect_is_not_duplicated_immediately(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(2200U);
    inputs.paired_device_available = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 2201U;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_FALSE(outputs.request_bt_reconnect);
}

static void test_second_reconnect_occurs_2s_after_first(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(2200U);
    inputs.paired_device_available = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 4200U;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_TRUE(outputs.request_bt_reconnect);
}

static void test_recovery_requires_stable_valid_source(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);

    ConnectivityInputs_t inputs = input_at(500U);
    inputs.bluetooth_connected = true;
    inputs.paired_device_available = true;
    ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);

    inputs.now_ms = 599U;
    outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
}

static void test_recovery_after_100ms_returns_to_m2(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);

    ConnectivityInputs_t inputs = input_at(500U);
    inputs.bluetooth_connected = true;
    inputs.paired_device_available = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 600U;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_NONE, outputs.fault);
}

static void test_fault_can_recover_to_telecoil(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);

    ConnectivityInputs_t inputs = input_at(500U);
    inputs.telecoil_valid = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 600U;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
}

static void test_fault_stays_m4_until_5s_without_valid_input(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(5199U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
}

static void test_fault_transitions_to_idle_at_5s_without_input(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(5200U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_NONE, outputs.fault);
}

static void test_intermittent_valid_source_resets_fault_to_idle_timer(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);

    ConnectivityInputs_t inputs = input_at(3000U);
    inputs.telecoil_valid = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 3050U;
    inputs.telecoil_valid = false;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 5200U;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
}

static void test_idle_wakes_to_bluetooth_when_link_appears(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(100U);
    inputs.interfaces_ready = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 200U;
    inputs.bluetooth_connected = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
}

static void test_disabling_active_bluetooth_is_not_a_fault(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_OK, Connectivity_SetEnabled(&controller, false, true, 150U));
    ConnectivityOutputs_t outputs;
    Connectivity_GetOutputs(&controller, &outputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_NONE, outputs.fault);
}

static void test_after_bluetooth_disabled_valid_telecoil_becomes_active(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    (void)Connectivity_SetEnabled(&controller, false, true, 150U);
    ConnectivityInputs_t inputs = input_at(150U);
    inputs.telecoil_valid = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
}

static void test_disabling_active_telecoil_is_not_a_fault(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_telecoil(&controller, 100U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_OK, Connectivity_SetEnabled(&controller, true, false, 150U));
    ConnectivityOutputs_t outputs;
    Connectivity_GetOutputs(&controller, &outputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_NONE, outputs.fault);
}

static void test_time_backwards_is_rejected_without_state_change(void)
{
    ConnectivityController_t controller = new_controller(100U);
    activate_bluetooth(&controller, 200U);
    ConnectivityInputs_t inputs = input_at(199U);
    inputs.bluetooth_connected = false;
    ConnectivityOutputs_t outputs;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_TIME_BACKWARDS,
                          Connectivity_Update(&controller, &inputs, &outputs));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, controller.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, controller.active_source);
}

static void test_link_loss_with_alternate_source_still_faults_before_recovery(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    ConnectivityInputs_t inputs = input_at(200U);
    inputs.telecoil_valid = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_FAULT_BT_LINK_LOSS, outputs.fault);
}

static void test_multistep_fault_reconnect_and_recovery_sequence(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    fault_bluetooth_link(&controller, 200U);

    ConnectivityInputs_t inputs = input_at(2199U);
    inputs.paired_device_available = true;
    ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_FALSE(outputs.request_bt_reconnect);

    inputs.now_ms = 2200U;
    outputs = update_ok(&controller, inputs);
    TEST_ASSERT_TRUE(outputs.request_bt_reconnect);

    inputs.now_ms = 2300U;
    inputs.telecoil_valid = true;
    outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);

    inputs.now_ms = 2400U;
    outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
    TEST_ASSERT_FALSE(outputs.muted);
}


static void test_set_enabled_rejects_null_controller(void)
{
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_ARGUMENT,
                          Connectivity_SetEnabled(NULL, true, true, 0U));
}

static void test_set_enabled_rejects_time_backwards(void)
{
    ConnectivityController_t controller = new_controller(100U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_TIME_BACKWARDS,
                          Connectivity_SetEnabled(&controller, true, true, 99U));
}

static void test_update_rejects_null_controller(void)
{
    ConnectivityInputs_t inputs = input_at(0U);
    ConnectivityOutputs_t outputs;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_ARGUMENT,
                          Connectivity_Update(NULL, &inputs, &outputs));
}

static void test_update_rejects_null_inputs(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityOutputs_t outputs;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_ARGUMENT,
                          Connectivity_Update(&controller, NULL, &outputs));
}

static void test_update_rejects_null_outputs(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(0U);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_ARGUMENT,
                          Connectivity_Update(&controller, &inputs, NULL));
}

static void test_defensive_active_bt_disabled_uses_valid_telecoil(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    controller.bluetooth_enabled = false; /* Simulate inconsistent external state. */
    ConnectivityInputs_t inputs = input_at(150U);
    inputs.telecoil_valid = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
}

static void test_defensive_active_bt_disabled_without_alternate_enters_idle(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_bluetooth(&controller, 100U);
    controller.bluetooth_enabled = false; /* Simulate inconsistent external state. */
    ConnectivityInputs_t inputs = input_at(150U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
}

static void test_defensive_active_telecoil_disabled_uses_valid_bluetooth(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_telecoil(&controller, 100U);
    controller.telecoil_enabled = false; /* Simulate inconsistent external state. */
    ConnectivityInputs_t inputs = input_at(150U);
    inputs.bluetooth_connected = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_BLUETOOTH, outputs.active_source);
}

static void test_defensive_active_telecoil_disabled_without_alternate_enters_idle(void)
{
    ConnectivityController_t controller = new_controller(0U);
    activate_telecoil(&controller, 100U);
    controller.telecoil_enabled = false; /* Simulate inconsistent external state. */
    ConnectivityInputs_t inputs = input_at(150U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
}

static void test_defensive_m2_with_no_source_selects_best_available(void)
{
    ConnectivityController_t controller = new_controller(0U);
    controller.mode = CONNECTIVITY_M2_ACTIVE;
    controller.active_source = AUDIO_SOURCE_NONE;
    ConnectivityInputs_t inputs = input_at(100U);
    inputs.telecoil_valid = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
}

static void test_defensive_m2_with_no_source_and_no_input_enters_idle(void)
{
    ConnectivityController_t controller = new_controller(0U);
    controller.mode = CONNECTIVITY_M2_ACTIVE;
    controller.active_source = AUDIO_SOURCE_NONE;
    ConnectivityInputs_t inputs = input_at(100U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
}

static void test_connection_timeout_fault_uses_reconnect_policy(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(5000U);
    inputs.interfaces_ready = true;
    inputs.paired_device_available = true;
    (void)update_ok(&controller, inputs);
    inputs.now_ms = 7000U;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_TRUE(outputs.request_bt_reconnect);
}

static void test_invalid_internal_mode_is_rejected(void)
{
    ConnectivityController_t controller = new_controller(0U);
    controller.mode = (ConnectivityMode_t)99;
    ConnectivityInputs_t inputs = input_at(100U);
    ConnectivityOutputs_t outputs;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ERR_CONFIG,
                          Connectivity_Update(&controller, &inputs, &outputs));
}

static void test_get_outputs_accepts_null_arguments(void)
{
    ConnectivityController_t controller = new_controller(0U);
    Connectivity_GetOutputs(NULL, NULL);
    Connectivity_GetOutputs(&controller, NULL);
    TEST_ASSERT_TRUE(true);
}


static void test_millisecond_timer_wrap_is_accepted(void)
{
    ConnectivityController_t controller = new_controller(UINT32_MAX - 50U);
    ConnectivityInputs_t inputs = input_at(UINT32_MAX - 10U);
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    (void)update_ok(&controller, inputs);

    inputs.now_ms = 25U; /* 36 ms later after the natural uint32_t wrap. */
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_EQUAL_INT(AUDIO_SOURCE_TELECOIL, outputs.active_source);
    TEST_ASSERT_EQUAL_UINT32(25U, controller.last_update_ms);
}


static void test_idle_requests_low_power(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(1U);
    inputs.interfaces_ready = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M3_IDLE, outputs.mode);
    TEST_ASSERT_TRUE(outputs.request_low_power);
}

static void test_active_mode_does_not_request_low_power(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(1U);
    inputs.interfaces_ready = true;
    inputs.bluetooth_connected = true;
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M2_ACTIVE, outputs.mode);
    TEST_ASSERT_FALSE(outputs.request_low_power);
}

static void test_fault_mode_does_not_request_low_power(void)
{
    ConnectivityController_t controller = new_controller(0U);
    ConnectivityInputs_t inputs = input_at(3000U);
    const ConnectivityOutputs_t outputs = update_ok(&controller, inputs);
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_M4_FAULT, outputs.mode);
    TEST_ASSERT_FALSE(outputs.request_low_power);
}

void RunConnectivityControllerTests(void)
{
    RUN_TEST(test_default_timing_matches_requirements);
    RUN_TEST(test_init_rejects_null_controller);
    RUN_TEST(test_init_rejects_invalid_recovery_timing);
    RUN_TEST(test_initial_state_is_m1_muted_with_no_source);
    RUN_TEST(test_startup_before_3s_remains_m1);
    RUN_TEST(test_startup_at_3s_without_interfaces_enters_fault);
    RUN_TEST(test_ready_without_any_source_enters_idle);
    RUN_TEST(test_bluetooth_source_activates_m2);
    RUN_TEST(test_telecoil_source_activates_m2);
    RUN_TEST(test_bluetooth_has_priority_when_both_sources_valid);
    RUN_TEST(test_paired_device_requests_initial_connection);
    RUN_TEST(test_connection_just_before_5s_has_not_timed_out);
    RUN_TEST(test_connection_at_5s_without_link_enters_fault);
    RUN_TEST(test_telecoil_switches_to_bluetooth_immediately_when_bt_arrives);
    RUN_TEST(test_bluetooth_link_loss_enters_m4);
    RUN_TEST(test_bluetooth_link_loss_mutes_immediately);
    RUN_TEST(test_invalid_active_telecoil_enters_m4);
    RUN_TEST(test_reconnect_not_requested_before_2s);
    RUN_TEST(test_reconnect_requested_exactly_at_2s);
    RUN_TEST(test_reconnect_is_not_duplicated_immediately);
    RUN_TEST(test_second_reconnect_occurs_2s_after_first);
    RUN_TEST(test_recovery_requires_stable_valid_source);
    RUN_TEST(test_recovery_after_100ms_returns_to_m2);
    RUN_TEST(test_fault_can_recover_to_telecoil);
    RUN_TEST(test_fault_stays_m4_until_5s_without_valid_input);
    RUN_TEST(test_fault_transitions_to_idle_at_5s_without_input);
    RUN_TEST(test_intermittent_valid_source_resets_fault_to_idle_timer);
    RUN_TEST(test_idle_wakes_to_bluetooth_when_link_appears);
    RUN_TEST(test_disabling_active_bluetooth_is_not_a_fault);
    RUN_TEST(test_after_bluetooth_disabled_valid_telecoil_becomes_active);
    RUN_TEST(test_disabling_active_telecoil_is_not_a_fault);
    RUN_TEST(test_time_backwards_is_rejected_without_state_change);
    RUN_TEST(test_link_loss_with_alternate_source_still_faults_before_recovery);
    RUN_TEST(test_multistep_fault_reconnect_and_recovery_sequence);
    RUN_TEST(test_set_enabled_rejects_null_controller);
    RUN_TEST(test_set_enabled_rejects_time_backwards);
    RUN_TEST(test_update_rejects_null_controller);
    RUN_TEST(test_update_rejects_null_inputs);
    RUN_TEST(test_update_rejects_null_outputs);
    RUN_TEST(test_defensive_active_bt_disabled_uses_valid_telecoil);
    RUN_TEST(test_defensive_active_bt_disabled_without_alternate_enters_idle);
    RUN_TEST(test_defensive_active_telecoil_disabled_uses_valid_bluetooth);
    RUN_TEST(test_defensive_active_telecoil_disabled_without_alternate_enters_idle);
    RUN_TEST(test_defensive_m2_with_no_source_selects_best_available);
    RUN_TEST(test_defensive_m2_with_no_source_and_no_input_enters_idle);
    RUN_TEST(test_connection_timeout_fault_uses_reconnect_policy);
    RUN_TEST(test_invalid_internal_mode_is_rejected);
    RUN_TEST(test_get_outputs_accepts_null_arguments);
    RUN_TEST(test_millisecond_timer_wrap_is_accepted);
    RUN_TEST(test_idle_requests_low_power);
    RUN_TEST(test_active_mode_does_not_request_low_power);
    RUN_TEST(test_fault_mode_does_not_request_low_power);
}
