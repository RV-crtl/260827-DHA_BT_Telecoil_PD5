#include "connectivity_actions.h"
#include "unity.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t connect;
    uint32_t reconnect;
    uint32_t enter_low_power;
    uint32_t exit_low_power;
    bool allow;
} ActionMock_t;

static bool on_connect(void *context)
{
    ActionMock_t *mock = (ActionMock_t *)context;
    ++mock->connect;
    return mock->allow;
}

static bool on_reconnect(void *context)
{
    ActionMock_t *mock = (ActionMock_t *)context;
    ++mock->reconnect;
    return mock->allow;
}

static bool on_enter(void *context)
{
    ActionMock_t *mock = (ActionMock_t *)context;
    ++mock->enter_low_power;
    return mock->allow;
}

static bool on_exit(void *context)
{
    ActionMock_t *mock = (ActionMock_t *)context;
    ++mock->exit_low_power;
    return mock->allow;
}

static ConnectivityActionPort_t make_port(void)
{
    ConnectivityActionPort_t port = {on_connect, on_reconnect, on_enter, on_exit};
    return port;
}

static void test_actions_init_rejects_null(void)
{
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_ARGUMENT, ConnectivityActions_Init(NULL));
}

static void test_actions_init_clears_state(void)
{
    ConnectivityActions_t state = {true, 1U, 2U, 3U, 4U};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK, ConnectivityActions_Init(&state));
    TEST_ASSERT_FALSE(state.low_power_engaged);
    TEST_ASSERT_EQUAL_UINT32(0U, state.connect_calls);
}

static void test_actions_apply_rejects_null_state(void)
{
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_ARGUMENT,
                          ConnectivityActions_Apply(NULL, &outputs, &port, NULL));
}

static void test_actions_apply_rejects_null_outputs(void)
{
    ConnectivityActions_t state = {0};
    const ConnectivityActionPort_t port = make_port();
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_ARGUMENT,
                          ConnectivityActions_Apply(&state, NULL, &port, NULL));
}

static void test_actions_apply_rejects_null_port(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_ARGUMENT,
                          ConnectivityActions_Apply(&state, &outputs, NULL, NULL));
}

static void test_actions_connect_request_calls_port_once(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    ActionMock_t mock = {0U, 0U, 0U, 0U, true};
    outputs.request_bt_connect = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_UINT32(1U, mock.connect);
    TEST_ASSERT_EQUAL_UINT32(1U, state.connect_calls);
}

static void test_actions_reconnect_request_calls_port_once(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    ActionMock_t mock = {0U, 0U, 0U, 0U, true};
    outputs.request_bt_reconnect = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_UINT32(1U, mock.reconnect);
    TEST_ASSERT_EQUAL_UINT32(1U, state.reconnect_calls);
}

static void test_actions_enter_low_power_is_edge_triggered(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    ActionMock_t mock = {0U, 0U, 0U, 0U, true};
    outputs.request_low_power = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_UINT32(1U, mock.enter_low_power);
}

static void test_actions_exit_low_power_is_edge_triggered(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    ActionMock_t mock = {0U, 0U, 0U, 0U, true};
    outputs.request_low_power = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    outputs.request_low_power = false;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_UINT32(1U, mock.exit_low_power);
    TEST_ASSERT_FALSE(state.low_power_engaged);
}

static void test_actions_null_callbacks_are_allowed(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = {0};
    outputs.request_bt_connect = true;
    outputs.request_bt_reconnect = true;
    outputs.request_low_power = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_OK,
                          ConnectivityActions_Apply(&state, &outputs, &port, NULL));
    TEST_ASSERT_TRUE(state.low_power_engaged);
}

static void test_actions_failed_connect_is_reported(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    ActionMock_t mock = {0U, 0U, 0U, 0U, false};
    outputs.request_bt_connect = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_CALLBACK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_UINT32(1U, mock.connect);
    TEST_ASSERT_EQUAL_UINT32(0U, state.connect_calls);
    outputs.request_bt_connect = false;
    outputs.request_bt_reconnect = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_CALLBACK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_EQUAL_UINT32(1U, mock.reconnect);
}

static void test_actions_failed_low_power_entry_does_not_latch_state(void)
{
    ConnectivityActions_t state = {0};
    ConnectivityOutputs_t outputs = {0};
    const ConnectivityActionPort_t port = make_port();
    ActionMock_t mock = {0U, 0U, 0U, 0U, false};
    outputs.request_low_power = true;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_CALLBACK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_FALSE(state.low_power_engaged);
    state.low_power_engaged = true;
    outputs.request_low_power = false;
    TEST_ASSERT_EQUAL_INT(CONNECTIVITY_ACTIONS_ERR_CALLBACK,
                          ConnectivityActions_Apply(&state, &outputs, &port, &mock));
    TEST_ASSERT_TRUE(state.low_power_engaged);
}

void RunConnectivityActionsTests(void)
{
    RUN_TEST(test_actions_init_rejects_null);
    RUN_TEST(test_actions_init_clears_state);
    RUN_TEST(test_actions_apply_rejects_null_state);
    RUN_TEST(test_actions_apply_rejects_null_outputs);
    RUN_TEST(test_actions_apply_rejects_null_port);
    RUN_TEST(test_actions_connect_request_calls_port_once);
    RUN_TEST(test_actions_reconnect_request_calls_port_once);
    RUN_TEST(test_actions_enter_low_power_is_edge_triggered);
    RUN_TEST(test_actions_exit_low_power_is_edge_triggered);
    RUN_TEST(test_actions_null_callbacks_are_allowed);
    RUN_TEST(test_actions_failed_connect_is_reported);
    RUN_TEST(test_actions_failed_low_power_entry_does_not_latch_state);
}
