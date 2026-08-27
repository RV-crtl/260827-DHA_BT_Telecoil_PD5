#include "control_protocol.h"
#include "unity.h"

#include <stddef.h>

static void expect_command(const char *text,
                           size_t length,
                           ConnectivityCommand_t expected)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_DISABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_OK,
                          ControlProtocol_Parse(text, length, &command));
    TEST_ASSERT_EQUAL_INT(expected, command);
}

static void test_bt_on_maps_to_enable_bluetooth(void)
{
    expect_command("BT ON", 5U, CONNECTIVITY_COMMAND_ENABLE_BLUETOOTH);
}

static void test_bt_off_maps_to_disable_bluetooth(void)
{
    expect_command("BT OFF", 6U, CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH);
}

static void test_tc_on_maps_to_enable_telecoil(void)
{
    expect_command("TC ON", 5U, CONNECTIVITY_COMMAND_ENABLE_TELECOIL);
}

static void test_tc_off_maps_to_disable_telecoil(void)
{
    expect_command("TC OFF", 6U, CONNECTIVITY_COMMAND_DISABLE_TELECOIL);
}

static void test_all_on_maps_to_enable_both(void)
{
    expect_command("ALL ON", 6U, CONNECTIVITY_COMMAND_ENABLE_BOTH);
}

static void test_all_off_maps_to_disable_both(void)
{
    expect_command("ALL OFF", 7U, CONNECTIVITY_COMMAND_DISABLE_BOTH);
}

static void test_parser_is_case_insensitive(void)
{
    expect_command("bt on", 5U, CONNECTIVITY_COMMAND_ENABLE_BLUETOOTH);
}

static void test_parser_accepts_surrounding_whitespace_and_crlf(void)
{
    expect_command(" \tTC OFF\r\n", 10U, CONNECTIVITY_COMMAND_DISABLE_TELECOIL);
}

static void test_status_is_reported_without_command_output(void)
{
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_STATUS_REQUEST,
                          ControlProtocol_Parse("STATUS\r\n", 8U, NULL));
}

static void test_status_rejects_extra_tokens(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("STATUS NOW", 10U, &command));
}

static void test_null_frame_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_ARGUMENT,
                          ControlProtocol_Parse(NULL, 4U, &command));
}

static void test_zero_length_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_ARGUMENT,
                          ControlProtocol_Parse("", 0U, &command));
}

static void test_blank_frame_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse(" \r\n", 3U, &command));
}

static void test_missing_action_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("BT", 2U, &command));
}

static void test_unknown_source_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("MIC ON", 6U, &command));
}

static void test_unknown_action_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("BT AUTO", 7U, &command));
}

static void test_extra_third_token_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("BT ON NOW", 9U, &command));
}

static void test_non_status_requires_command_output(void)
{
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_ARGUMENT,
                          ControlProtocol_Parse("BT ON", 5U, NULL));
}


static void test_token_longer_than_known_literal_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("STATUSX", 7U, &command));
}


static void test_mode_auto_maps_to_enable_both(void)
{
    expect_command("MODE AUTO", 9U, CONNECTIVITY_COMMAND_ENABLE_BOTH);
}

static void test_mode_bt_maps_to_bluetooth_only(void)
{
    expect_command("MODE BT", 7U, CONNECTIVITY_COMMAND_BLUETOOTH_ONLY);
}

static void test_mode_tc_maps_to_telecoil_only(void)
{
    expect_command("MODE TC", 7U, CONNECTIVITY_COMMAND_TELECOIL_ONLY);
}

static void test_mode_off_maps_to_disable_both(void)
{
    expect_command("MODE OFF", 8U, CONNECTIVITY_COMMAND_DISABLE_BOTH);
}

static void test_mode_unknown_policy_is_rejected(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    TEST_ASSERT_EQUAL_INT(CONTROL_PROTOCOL_ERR_FORMAT,
                          ControlProtocol_Parse("MODE FAULT", 10U, &command));
}

void RunControlProtocolTests(void)
{
    RUN_TEST(test_bt_on_maps_to_enable_bluetooth);
    RUN_TEST(test_bt_off_maps_to_disable_bluetooth);
    RUN_TEST(test_tc_on_maps_to_enable_telecoil);
    RUN_TEST(test_tc_off_maps_to_disable_telecoil);
    RUN_TEST(test_all_on_maps_to_enable_both);
    RUN_TEST(test_all_off_maps_to_disable_both);
    RUN_TEST(test_mode_auto_maps_to_enable_both);
    RUN_TEST(test_mode_bt_maps_to_bluetooth_only);
    RUN_TEST(test_mode_tc_maps_to_telecoil_only);
    RUN_TEST(test_mode_off_maps_to_disable_both);
    RUN_TEST(test_mode_unknown_policy_is_rejected);
    RUN_TEST(test_parser_is_case_insensitive);
    RUN_TEST(test_parser_accepts_surrounding_whitespace_and_crlf);
    RUN_TEST(test_status_is_reported_without_command_output);
    RUN_TEST(test_status_rejects_extra_tokens);
    RUN_TEST(test_null_frame_is_rejected);
    RUN_TEST(test_zero_length_is_rejected);
    RUN_TEST(test_blank_frame_is_rejected);
    RUN_TEST(test_missing_action_is_rejected);
    RUN_TEST(test_unknown_source_is_rejected);
    RUN_TEST(test_unknown_action_is_rejected);
    RUN_TEST(test_extra_third_token_is_rejected);
    RUN_TEST(test_non_status_requires_command_output);
    RUN_TEST(test_token_longer_than_known_literal_is_rejected);
}
