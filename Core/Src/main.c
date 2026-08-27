/**
 * @file main.c
 * @brief STM32F446RE entry point selecting Debug, Testing or SelfTest execution.
 *
 * The normal Debug build runs a deterministic demonstration using simulated link/audio
 * observations. UNIT_TEST selects the 256-test automated suite; SELF_TEST selects the
 * independent 52-check regression suite. The hearing-aid application itself remains in
 * `Application/` and contains no STM32 HAL or peripheral-register dependency.
 */
#include "board_bt401_uart.h"
#include "board_console.h"
#include "board_time.h"
#include "bt_profile.h"
#include "connectivity_service.h"
#include "control_protocol.h"
#include "requirements.h"
#ifdef UNIT_TEST
#include "test_runner.h"
#endif
#ifdef SELF_TEST
#include "self_test_runner.h"
#endif

#include <stdbool.h>
#include <stdint.h>

#if !defined(UNIT_TEST) && !defined(SELF_TEST)
#define ENABLE_BT401_HARDWARE_PROFILE 0U
#endif

#if !defined(UNIT_TEST) && !defined(SELF_TEST)
/**
 * @brief Render an audio-source enum to the serial demonstration console.
 * @param source Source value to print.
 */
static void print_source(ConnectivityAudioSource_t source)
{
    if (source == AUDIO_SOURCE_BLUETOOTH) {
        BoardConsole_WriteString("Bluetooth");
    } else if (source == AUDIO_SOURCE_TELECOIL) {
        BoardConsole_WriteString("Telecoil");
    } else {
        BoardConsole_WriteString("None");
    }
}

/**
 * @brief Render an M1-M4 mode enum to the serial demonstration console.
 * @param mode Mode value to print.
 */
static void print_mode(ConnectivityMode_t mode)
{
    switch (mode) {
    case CONNECTIVITY_M1_INITIALISING: BoardConsole_WriteString("M1 Initialising"); break;
    case CONNECTIVITY_M2_ACTIVE:       BoardConsole_WriteString("M2 Active"); break;
    case CONNECTIVITY_M3_IDLE:         BoardConsole_WriteString("M3 Idle"); break;
    case CONNECTIVITY_M4_FAULT:        BoardConsole_WriteString("M4 Fault"); break;
    default:                            BoardConsole_WriteString("Invalid"); break;
    }
}

/**
 * @brief Print one concise mode/source/mute snapshot for the Debug demonstration.
 * @param label Human-readable scenario label.
 * @param service Initialised service to inspect.
 */
static void print_status(const char *label, const ConnectivityService_t *service)
{
    ConnectivityOutputs_t outputs;
    if (ConnectivityService_GetStatus(service, &outputs) != CONNECTIVITY_SERVICE_OK) {
        BoardConsole_WriteString("Status read error\r\n");
        return;
    }
    BoardConsole_WriteString(label);
    BoardConsole_WriteString(" | mode=");
    print_mode(outputs.mode);
    BoardConsole_WriteString(" | source=");
    print_source(outputs.active_source);
    BoardConsole_WriteString(outputs.muted ? " | MUTED\r\n" : " | audio enabled\r\n");
}

/**
 * @brief Run deterministic hardware-independent scenarios on the F446 execution target.
 *
 * Simulated observations demonstrate telecoil activation, Bluetooth priority, stale-frame
 * rejection, M4 mute, recovery, command parsing and service-cadence diagnostics. No real
 * Bluetooth/telecoil hardware is required for this PD5b demonstration.
 */
static void RunApplicationDemo(void)
{
    BoardConsole_WriteString("\r\n=== 260827-DHA_BT_Telecoil_PD5 application ===\r\n");
    BoardConsole_WriteString("Target: NUCLEO-F446RE | portable application logic is HAL-free\r\n");

#if ENABLE_BT401_HARDWARE_PROFILE
    BoardBt401Uart_Init();
    const BtProfileConfig_t bt_config = BtProfile_DefaultConfig();
    size_t sent = 0U;
    const BtProfileResult_t bt_result = BtProfile_Apply(&bt_config,
                                                        BoardBt401Uart_Send,
                                                        BoardBt401Uart_Delay,
                                                        NULL,
                                                        &sent);
    BoardConsole_WriteString((bt_result == BT_PROFILE_OK) ?
                             "BT401 profile: transmitted\r\n" :
                             "BT401 profile: transport error\r\n");
#endif

    ConnectivityService_t service;
    if (ConnectivityService_Init(&service, REQ_SAMPLE_RATE_48K_HZ, 0U) != CONNECTIVITY_SERVICE_OK) {
        BoardConsole_WriteString("Application service initialisation FAILED\r\n");
        BoardConsole_SetLed(false);
        return;
    }
    (void)ConnectivityService_SetTelecoilNoiseFloor(&service, 200U);

    ConnectivityLinkState_t link = {1U, true, true, false};
    (void)ConnectivityService_Step(&service, &link);
    print_status("Ready, no source", &service);

    int16_t telecoil_in[32];
    int16_t telecoil_out[32];
    for (uint32_t i = 0U; i < 32U; ++i) {
        telecoil_in[i] = ((i & 1U) == 0U) ? 2000 : -2000;
    }
    (void)ConnectivityService_ProcessTelecoilBlock(&service, telecoil_in, telecoil_out, 32U, 2U);
    (void)ConnectivityService_ProcessTelecoilBlock(&service, telecoil_in, telecoil_out, 32U, 3U);
    print_status("Telecoil detected", &service);

    link.now_ms = 4U;
    link.bluetooth_connected = true;
    (void)ConnectivityService_Step(&service, &link);
    print_status("Bluetooth becomes valid (priority)", &service);

    const int16_t bt_in[4] = {1000, -1000, 2000, -2000};
    int16_t bt_out[4] = {0, 0, 0, 0};
    const ConnectivityServiceResult_t fresh = ConnectivityService_ProcessBluetoothBlock(
        &service, bt_in, bt_out, 4U, 4U, 5U);
    BoardConsole_WriteString((fresh == CONNECTIVITY_SERVICE_OK) ?
                             "Fresh Bluetooth frame accepted\r\n" :
                             "Fresh Bluetooth frame unexpectedly rejected\r\n");

    link.now_ms = 45U;
    (void)ConnectivityService_Step(&service, &link);
    link.now_ms = 85U;
    (void)ConnectivityService_Step(&service, &link);

    const ConnectivityServiceResult_t stale = ConnectivityService_ProcessBluetoothBlock(
        &service, bt_in, bt_out, 4U, 4U, 105U);
    BoardConsole_WriteString((stale == CONNECTIVITY_SERVICE_STALE_BT_FRAME) ?
                             "101 ms Bluetooth frame correctly rejected by latency guard\r\n" :
                             "Latency guard check FAILED\r\n");

    link.now_ms = 106U;
    link.bluetooth_connected = false;
    (void)ConnectivityService_Step(&service, &link);
    print_status("Bluetooth link lost", &service);

    (void)ConnectivityService_ProcessTelecoilBlock(&service, telecoil_in, telecoil_out, 32U, 107U);
    link.now_ms = 147U;
    (void)ConnectivityService_Step(&service, &link);
    link.now_ms = 187U;
    (void)ConnectivityService_Step(&service, &link);
    link.now_ms = 207U;
    (void)ConnectivityService_Step(&service, &link);
    print_status("Stable telecoil recovery", &service);

    ConnectivityCommand_t parsed_command;
    if (ControlProtocol_Parse("BT OFF\r\n", 8U, &parsed_command) == CONTROL_PROTOCOL_OK) {
        (void)ConnectivityService_ApplyCommand(&service, parsed_command, 208U);
        print_status("UART-style control: BT OFF", &service);
    } else {
        BoardConsole_WriteString("Control protocol parse FAILED\r\n");
    }

    BoardConsole_WriteString("Diagnostics: updates=");
    BoardConsole_WriteUInt(service.diagnostics.controller_updates);
    BoardConsole_WriteString(" switches=");
    BoardConsole_WriteUInt(service.diagnostics.source_switches);
    BoardConsole_WriteString(" faults=");
    BoardConsole_WriteUInt(service.diagnostics.faults_entered);
    BoardConsole_WriteString(" recoveries=");
    BoardConsole_WriteUInt(service.diagnostics.recoveries);
    BoardConsole_WriteString(" stale_bt_frames=");
    BoardConsole_WriteUInt(service.diagnostics.stale_bt_frames_dropped);
    BoardConsole_WriteString(" deadline_misses=");
    BoardConsole_WriteUInt(service.diagnostics.service_deadline_misses);
    BoardConsole_WriteString(" max_service_interval_ms=");
    BoardConsole_WriteUInt(service.diagnostics.max_service_interval_ms);
    BoardConsole_WriteString("\r\n");

    BoardConsole_WriteString("Application demonstration completed.\r\n");
    BoardConsole_WriteString("Use Testing for the comprehensive unit suite; SelfTest for independent checks.\r\n");
    BoardConsole_SetLed(true);
}
#endif

/**
 * @brief Initialise the minimal board adapter and execute the active build configuration.
 * @return This bare-metal entry point does not return.
 */
int main(void)
{
    BoardConsole_Init();
    BoardTime_Init();

#if defined(UNIT_TEST)
    const int failures = RunAllTests(BoardConsole_WriteString);
    BoardConsole_SetLed(failures == 0);
    BoardConsole_WriteString("\r\nUnity Testing build finished. Green LED = all tests passed.\r\n");
#elif defined(SELF_TEST)
    const int failures = SelfTest_RunAll(BoardConsole_WriteString);
    BoardConsole_SetLed(failures == 0);
    BoardConsole_WriteString("\r\nFramework-free SelfTest build finished. Green LED = all checks passed.\r\n");
#else
    RunApplicationDemo();
#endif

    for (;;) {
        /* Deliberately idle: Prototype Design 5b excludes RTOS / real-time integration. */
    }
}
