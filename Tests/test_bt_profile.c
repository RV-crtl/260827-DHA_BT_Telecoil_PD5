#include "bt_profile.h"
#include "unity.h"

#include <string.h>

#define MOCK_MAX_COMMANDS 12U

typedef struct {
    char commands[MOCK_MAX_COMMANDS][20];
    size_t command_count;
    uint32_t delays[MOCK_MAX_COMMANDS];
    size_t delay_count;
    size_t fail_at_command;
} BtMock_t;

static bool mock_send(const char *command, size_t length, void *context)
{
    BtMock_t *const mock = (BtMock_t *)context;
    const size_t call_index = mock->command_count;
    if (call_index == mock->fail_at_command) {
        return false;
    }
    if (call_index >= MOCK_MAX_COMMANDS) {
        return false;
    }
    const size_t copy_len = (length < (sizeof(mock->commands[0]) - 1U)) ? length : (sizeof(mock->commands[0]) - 1U);
    memcpy(mock->commands[call_index], command, copy_len);
    mock->commands[call_index][copy_len] = '\0';
    mock->command_count++;
    return true;
}

static void mock_delay(uint32_t delay_ms, void *context)
{
    BtMock_t *const mock = (BtMock_t *)context;
    if (mock->delay_count < MOCK_MAX_COMMANDS) {
        mock->delays[mock->delay_count++] = delay_ms;
    }
}

static BtMock_t new_mock(void)
{
    BtMock_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.fail_at_command = (size_t)-1;
    return mock;
}

static void test_default_profile_delay_is_100ms(void)
{
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    TEST_ASSERT_EQUAL_UINT32(100U, config.inter_command_delay_ms);
}

static void test_command_count_and_bounds(void)
{
    TEST_ASSERT_EQUAL_UINT(9U, BT_PROFILE_COMMAND_COUNT);
    TEST_ASSERT_NOT_NULL(BtProfile_CommandAt(0U));
    TEST_ASSERT_NOT_NULL(BtProfile_CommandAt(8U));
    TEST_ASSERT_NULL(BtProfile_CommandAt(9U));
}

static void test_profile_preserves_original_command_sequence(void)
{
    BtMock_t mock = new_mock();
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    size_t sent = 0U;

    TEST_ASSERT_EQUAL_INT(BT_PROFILE_OK, BtProfile_Apply(&config, mock_send, mock_delay, &mock, &sent));
    TEST_ASSERT_EQUAL_SIZE_T(BT_PROFILE_COMMAND_COUNT, sent);
    TEST_ASSERT_EQUAL_STRING("AT+CM01\r\n", mock.commands[0]);
    TEST_ASSERT_EQUAL_STRING("AT+B200\r\n", mock.commands[1]);
    TEST_ASSERT_EQUAL_STRING("AT+B301\r\n", mock.commands[2]);
    TEST_ASSERT_EQUAL_STRING("AT+B501\r\n", mock.commands[3]);
    TEST_ASSERT_EQUAL_STRING("AT+B401\r\n", mock.commands[4]);
    TEST_ASSERT_EQUAL_STRING("AT+CN00\r\n", mock.commands[5]);
    TEST_ASSERT_EQUAL_STRING("AT+CA30\r\n", mock.commands[6]);
    TEST_ASSERT_EQUAL_STRING("AT+CU00\r\n", mock.commands[7]);
    TEST_ASSERT_EQUAL_STRING("AT+CS01\r\n", mock.commands[8]);
}

static void test_profile_delays_between_commands_only(void)
{
    BtMock_t mock = new_mock();
    BtProfileConfig_t config = BtProfile_DefaultConfig();
    config.inter_command_delay_ms = 25U;

    TEST_ASSERT_EQUAL_INT(BT_PROFILE_OK, BtProfile_Apply(&config, mock_send, mock_delay, &mock, NULL));
    TEST_ASSERT_EQUAL_SIZE_T(BT_PROFILE_COMMAND_COUNT - 1U, mock.delay_count);
    for (size_t i = 0U; i < mock.delay_count; ++i) {
        TEST_ASSERT_EQUAL_UINT32(25U, mock.delays[i]);
    }
}

static void test_zero_delay_skips_delay_callback(void)
{
    BtMock_t mock = new_mock();
    BtProfileConfig_t config = BtProfile_DefaultConfig();
    config.inter_command_delay_ms = 0U;

    TEST_ASSERT_EQUAL_INT(BT_PROFILE_OK, BtProfile_Apply(&config, mock_send, mock_delay, &mock, NULL));
    TEST_ASSERT_EQUAL_SIZE_T(0U, mock.delay_count);
}

static void test_null_delay_callback_is_allowed(void)
{
    BtMock_t mock = new_mock();
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(BT_PROFILE_OK, BtProfile_Apply(&config, mock_send, NULL, &mock, NULL));
    TEST_ASSERT_EQUAL_SIZE_T(BT_PROFILE_COMMAND_COUNT, mock.command_count);
}

static void test_send_failure_aborts_sequence_and_reports_progress(void)
{
    BtMock_t mock = new_mock();
    mock.fail_at_command = 3U;
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    size_t sent = 999U;

    TEST_ASSERT_EQUAL_INT(BT_PROFILE_ERR_SEND, BtProfile_Apply(&config, mock_send, mock_delay, &mock, &sent));
    TEST_ASSERT_EQUAL_SIZE_T(3U, sent);
    TEST_ASSERT_EQUAL_SIZE_T(3U, mock.command_count);
}

static void test_null_config_is_rejected(void)
{
    BtMock_t mock = new_mock();
    TEST_ASSERT_EQUAL_INT(BT_PROFILE_ERR_ARGUMENT, BtProfile_Apply(NULL, mock_send, mock_delay, &mock, NULL));
}

static void test_null_send_function_is_rejected(void)
{
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    TEST_ASSERT_EQUAL_INT(BT_PROFILE_ERR_ARGUMENT, BtProfile_Apply(&config, NULL, NULL, NULL, NULL));
}

void RunBtProfileTests(void)
{
    RUN_TEST(test_default_profile_delay_is_100ms);
    RUN_TEST(test_command_count_and_bounds);
    RUN_TEST(test_profile_preserves_original_command_sequence);
    RUN_TEST(test_profile_delays_between_commands_only);
    RUN_TEST(test_zero_delay_skips_delay_callback);
    RUN_TEST(test_null_delay_callback_is_allowed);
    RUN_TEST(test_send_failure_aborts_sequence_and_reports_progress);
    RUN_TEST(test_null_config_is_rejected);
    RUN_TEST(test_null_send_function_is_rejected);
}
