/**
 * @file self_test_runner.c
 * @brief Supplementary framework-free regression checks for critical application behaviour.
 */
#include "self_test_runner.h"

#include "audio_processing.h"
#include "audio_dynamics.h"
#include "bt_profile.h"
#include "connectivity_controller.h"
#include "connectivity_actions.h"
#include "connectivity_service.h"
#include "control_protocol.h"
#include "pcm_transport.h"
#include "requirements.h"
#include "signal_quality.h"
#include "telecoil_detector.h"
#include "telecoil_filter.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef bool (*SelfTestFn)(void);

typedef struct {
    const char *name;
    SelfTestFn function;
} SelfTestCase_t;

typedef struct {
    size_t sends;
    size_t delays;
    size_t fail_at;
    uint32_t last_delay_ms;
    bool order_ok;
} BtMock_t;

static SelfTestOutputFn g_output;

static void out(const char *text)
{
    if ((g_output != NULL) && (text != NULL)) {
        g_output(text);
    }
}

static void out_uint(size_t value)
{
    char digits[24];
    size_t count = 0U;
    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));

    while (count > 0U) {
        char text[2] = {digits[--count], '\0'};
        out(text);
    }
}

static uint32_t abs_float_scaled(float value)
{
    if (value < 0.0f) {
        value = -value;
    }
    if (value > 1000000.0f) {
        return UINT32_MAX;
    }
    return (uint32_t)(value * 1000.0f);
}

static bool bt_mock_send(const char *command, size_t length, void *context)
{
    BtMock_t *const mock = (BtMock_t *)context;
    if ((mock == NULL) || (command == NULL) || (length == 0U)) {
        return false;
    }

    if ((mock->fail_at != SIZE_MAX) && (mock->sends == mock->fail_at)) {
        return false;
    }

    const char *const expected = BtProfile_CommandAt(mock->sends);
    if ((expected == NULL) || (strlen(expected) != length) ||
        (memcmp(expected, command, length) != 0)) {
        mock->order_ok = false;
    }
    ++mock->sends;
    return true;
}

static void bt_mock_delay(uint32_t delay_ms, void *context)
{
    BtMock_t *const mock = (BtMock_t *)context;
    if (mock != NULL) {
        ++mock->delays;
        mock->last_delay_ms = delay_ms;
    }
}

static bool init_controller(ConnectivityController_t *controller)
{
    const ConnectivityConfig_t config = Connectivity_DefaultConfig();
    return Connectivity_Init(controller, &config, 0U) == CONNECTIVITY_OK;
}

static bool make_bt_active(ConnectivityController_t *controller, uint32_t at_ms)
{
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    inputs.now_ms = at_ms;
    inputs.interfaces_ready = true;
    inputs.bluetooth_connected = true;
    return (Connectivity_Update(controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (outputs.active_source == AUDIO_SOURCE_BLUETOOTH) && !outputs.muted;
}

static bool make_telecoil_active(ConnectivityController_t *controller, uint32_t at_ms)
{
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    inputs.now_ms = at_ms;
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    return (Connectivity_Update(controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (outputs.active_source == AUDIO_SOURCE_TELECOIL) && !outputs.muted;
}



static ConnectivityLinkState_t service_link(uint32_t now_ms,
                                             bool ready,
                                             bool paired,
                                             bool connected)
{
    ConnectivityLinkState_t state;
    state.now_ms = now_ms;
    state.interfaces_ready = ready;
    state.paired_device_available = paired;
    state.bluetooth_connected = connected;
    return state;
}

static void fill_strong_telecoil(int16_t *samples, size_t count, int16_t amplitude)
{
    if (samples == NULL) {
        return;
    }
    for (size_t i = 0U; i < count; ++i) {
        samples[i] = ((i & 1U) == 0U) ? amplitude : (int16_t)-amplitude;
    }
}

static bool st_sample_rates(void)
{
    return Audio_IsSupportedSampleRate(16000U) &&
           Audio_IsSupportedSampleRate(48000U) &&
           !Audio_IsSupportedSampleRate(44100U);
}

static bool st_pcm_decode_extremes(void)
{
    return (Audio_DecodeS24ToS16(0x007FFFFFUL) == INT16_MAX) &&
           (Audio_DecodeS24ToS16(0x00800000UL) == INT16_MIN) &&
           (Audio_DecodeS24ToS16(0U) == 0);
}

static bool st_gain_and_saturation(void)
{
    return (Audio_ApplyGainQ15(12000, 32768) == 12000) &&
           (Audio_ApplyGainQ15(12000, 16384) == 6000) &&
           (Audio_ApplyGainQ15(30000, 65536) == INT16_MAX) &&
           (Audio_ApplyGainQ15(-30000, 65536) == INT16_MIN) &&
           (Audio_ApplyGainQ15(1234, -1) == 0);
}

static bool st_mix_and_mute(void)
{
    return (Audio_MixStereoToMono(2000, 1000) == 1500) &&
           (Audio_MixStereoToMono(INT16_MAX, INT16_MAX) == INT16_MAX) &&
           (Audio_ApplyMute(1234, false) == 1234) &&
           (Audio_ApplyMute(1234, true) == 0);
}

static bool st_audio_block(void)
{
    const int16_t stereo[] = {1000, 3000, -1000, -3000};
    int16_t mono[2] = {0, 0};
    if (!Audio_ProcessStereoToMono(stereo, mono, 2U, 32768, false)) {
        return false;
    }
    return (mono[0] == 2000) && (mono[1] == -2000) &&
           Audio_ProcessStereoToMono(NULL, NULL, 0U, 32768, false) &&
           !Audio_ProcessStereoToMono(stereo, mono, 2U, -1, false);
}

static bool st_bt_command_table(void)
{
    const char *first = BtProfile_CommandAt(0U);
    const char *last = BtProfile_CommandAt(BT_PROFILE_COMMAND_COUNT - 1U);
    return (first != NULL) && (last != NULL) &&
           (strcmp(first, "AT+CM01\r\n") == 0) &&
           (strcmp(last, "AT+CS01\r\n") == 0) &&
           (BtProfile_CommandAt(BT_PROFILE_COMMAND_COUNT) == NULL);
}

static bool st_bt_profile_success(void)
{
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    BtMock_t mock = {0U, 0U, SIZE_MAX, 0U, true};
    size_t sent = 0U;
    const BtProfileResult_t result = BtProfile_Apply(&config,
                                                      bt_mock_send,
                                                      bt_mock_delay,
                                                      &mock,
                                                      &sent);
    return (result == BT_PROFILE_OK) && (sent == BT_PROFILE_COMMAND_COUNT) &&
           (mock.sends == BT_PROFILE_COMMAND_COUNT) &&
           (mock.delays == (BT_PROFILE_COMMAND_COUNT - 1U)) &&
           (mock.last_delay_ms == config.inter_command_delay_ms) && mock.order_ok;
}

static bool st_bt_profile_failure(void)
{
    const BtProfileConfig_t config = BtProfile_DefaultConfig();
    BtMock_t mock = {0U, 0U, 3U, 0U, true};
    size_t sent = 99U;
    const BtProfileResult_t result = BtProfile_Apply(&config,
                                                      bt_mock_send,
                                                      bt_mock_delay,
                                                      &mock,
                                                      &sent);
    return (result == BT_PROFILE_ERR_SEND) && (sent == 3U) && (mock.sends == 3U);
}

static bool st_connectivity_init(void)
{
    ConnectivityController_t controller;
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller)) {
        return false;
    }
    Connectivity_GetOutputs(&controller, &outputs);
    return (outputs.mode == CONNECTIVITY_M1_INITIALISING) &&
           (outputs.active_source == AUDIO_SOURCE_NONE) && outputs.muted;
}

static bool st_telecoil_becomes_active(void)
{
    ConnectivityController_t controller;
    return init_controller(&controller) && make_telecoil_active(&controller, 100U);
}

static bool st_bluetooth_priority(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller) || !make_telecoil_active(&controller, 100U)) {
        return false;
    }
    inputs.now_ms = 101U;
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    inputs.bluetooth_connected = true;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.active_source == AUDIO_SOURCE_BLUETOOTH);
}

static bool st_bt_link_loss_fault_and_mute(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller) || !make_bt_active(&controller, 100U)) {
        return false;
    }
    inputs.now_ms = 101U;
    inputs.interfaces_ready = true;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M4_FAULT) && outputs.muted &&
           (outputs.fault == CONNECTIVITY_FAULT_BT_LINK_LOSS);
}

static bool st_reconnect_exact_boundary(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller) || !make_bt_active(&controller, 100U)) {
        return false;
    }
    inputs.now_ms = 101U;
    inputs.paired_device_available = true;
    (void)Connectivity_Update(&controller, &inputs, &outputs);

    inputs.now_ms = 2100U;
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        outputs.request_bt_reconnect) {
        return false;
    }
    inputs.now_ms = 2101U;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           outputs.request_bt_reconnect;
}

static bool st_recovery_stability(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller) || !make_bt_active(&controller, 100U)) {
        return false;
    }
    inputs.now_ms = 101U;
    (void)Connectivity_Update(&controller, &inputs, &outputs);
    if (outputs.mode != CONNECTIVITY_M4_FAULT) {
        return false;
    }

    inputs.now_ms = 200U;
    inputs.telecoil_valid = true;
    (void)Connectivity_Update(&controller, &inputs, &outputs);
    if (outputs.mode != CONNECTIVITY_M4_FAULT) {
        return false;
    }
    inputs.now_ms = 299U;
    (void)Connectivity_Update(&controller, &inputs, &outputs);
    if (outputs.mode != CONNECTIVITY_M4_FAULT) {
        return false;
    }
    inputs.now_ms = 300U;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (outputs.active_source == AUDIO_SOURCE_TELECOIL) && !outputs.muted;
}

static bool st_fault_to_idle_timeout(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller) || !make_bt_active(&controller, 100U)) {
        return false;
    }
    inputs.now_ms = 101U;
    (void)Connectivity_Update(&controller, &inputs, &outputs);
    inputs.now_ms = 5100U;
    (void)Connectivity_Update(&controller, &inputs, &outputs);
    if (outputs.mode != CONNECTIVITY_M4_FAULT) {
        return false;
    }
    inputs.now_ms = 5101U;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M3_IDLE) && outputs.muted;
}

static bool st_startup_timeout(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller)) {
        return false;
    }
    inputs.now_ms = 2999U;
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        (outputs.mode != CONNECTIVITY_M1_INITIALISING)) {
        return false;
    }
    inputs.now_ms = 3000U;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M4_FAULT) &&
           (outputs.fault == CONNECTIVITY_FAULT_STARTUP_TIMEOUT);
}

static bool st_bt_connection_timeout(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller)) {
        return false;
    }
    inputs.interfaces_ready = true;
    inputs.paired_device_available = true;
    inputs.now_ms = 4999U;
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        !outputs.request_bt_connect || (outputs.mode != CONNECTIVITY_M1_INITIALISING)) {
        return false;
    }
    inputs.now_ms = 5000U;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M4_FAULT) &&
           (outputs.fault == CONNECTIVITY_FAULT_BT_CONNECTION_TIMEOUT);
}

static bool st_intentional_disable_not_fault(void)
{
    ConnectivityController_t controller;
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller) || !make_bt_active(&controller, 100U)) {
        return false;
    }
    if (Connectivity_SetEnabled(&controller, false, true, 101U) != CONNECTIVITY_OK) {
        return false;
    }
    Connectivity_GetOutputs(&controller, &outputs);
    return (outputs.mode == CONNECTIVITY_M3_IDLE) &&
           (outputs.fault == CONNECTIVITY_FAULT_NONE) && outputs.muted;
}

static bool st_timer_rollover(void)
{
    ConnectivityController_t controller;
    const ConnectivityConfig_t config = Connectivity_DefaultConfig();
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    const uint32_t start = UINT32_MAX - 1000U;
    if (Connectivity_Init(&controller, &config, start) != CONNECTIVITY_OK) {
        return false;
    }
    inputs.interfaces_ready = false;
    inputs.now_ms = 1500U; /* 2501 ms elapsed across wrap */
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        (outputs.mode != CONNECTIVITY_M1_INITIALISING)) {
        return false;
    }
    inputs.now_ms = 2000U; /* 3001 ms elapsed across wrap */
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M4_FAULT);
}

static bool st_detector_silence(void)
{
    TelecoilDetector_t detector;
    const TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    const int16_t samples[32] = {0};
    TelecoilBlockMetrics_t metrics;
    return (TelecoilDetector_Init(&detector, &config) == TELECOIL_DETECTOR_OK) &&
           (TelecoilDetector_Process(&detector, samples, 32U, &metrics) == TELECOIL_DETECTOR_OK) &&
           !metrics.valid_signal && !metrics.raw_candidate_present;
}

static bool st_detector_hysteresis(void)
{
    TelecoilDetector_t detector;
    const TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    int16_t strong[32];
    int16_t quiet[32] = {0};
    TelecoilBlockMetrics_t metrics;
    for (size_t i = 0U; i < 32U; ++i) {
        strong[i] = ((i & 1U) == 0U) ? 2000 : -2000;
    }
    if (TelecoilDetector_Init(&detector, &config) != TELECOIL_DETECTOR_OK) {
        return false;
    }
    (void)TelecoilDetector_Process(&detector, strong, 32U, &metrics);
    if (metrics.valid_signal) {
        return false;
    }
    (void)TelecoilDetector_Process(&detector, strong, 32U, &metrics);
    if (!metrics.valid_signal || !metrics.state_changed) {
        return false;
    }
    (void)TelecoilDetector_Process(&detector, quiet, 32U, &metrics);
    (void)TelecoilDetector_Process(&detector, quiet, 32U, &metrics);
    if (!metrics.valid_signal) {
        return false;
    }
    (void)TelecoilDetector_Process(&detector, quiet, 32U, &metrics);
    return !metrics.valid_signal && metrics.state_changed;
}

static bool st_detector_clipping_rejected(void)
{
    TelecoilDetector_t detector;
    const TelecoilDetectorConfig_t config = TelecoilDetector_DefaultConfig();
    int16_t clipped[32];
    TelecoilBlockMetrics_t metrics;
    for (size_t i = 0U; i < 32U; ++i) {
        clipped[i] = ((i & 1U) == 0U) ? INT16_MAX : INT16_MIN;
    }
    if (TelecoilDetector_Init(&detector, &config) != TELECOIL_DETECTOR_OK) {
        return false;
    }
    return (TelecoilDetector_Process(&detector, clipped, 32U, &metrics) == TELECOIL_DETECTOR_OK) &&
           !metrics.raw_candidate_present && !metrics.valid_signal &&
           (metrics.clipped_samples == 32U);
}

static bool st_filter_supported_rates(void)
{
    TelecoilFilter_t filter;
    return TelecoilFilter_Init(&filter, 16000U) &&
           TelecoilFilter_Init(&filter, 48000U) &&
           !TelecoilFilter_Init(&filter, 44100U);
}

static bool st_filter_impulse_stable(void)
{
    TelecoilFilter_t filter;
    if (!TelecoilFilter_Init(&filter, 48000U)) {
        return false;
    }
    float maximum = 0.0f;
    for (size_t i = 0U; i < 512U; ++i) {
        const float input = (i == 0U) ? 1.0f : 0.0f;
        const float output = TelecoilFilter_ProcessSample(&filter, input);
        const float magnitude = (output < 0.0f) ? -output : output;
        if (magnitude > maximum) {
            maximum = magnitude;
        }
        if (abs_float_scaled(output) == UINT32_MAX) {
            return false;
        }
    }
    return (maximum < 10.0f) && (abs_float_scaled(TelecoilFilter_ProcessSample(&filter, 0.0f)) < 10U);
}

static bool st_filter_block_and_reset(void)
{
    TelecoilFilter_t filter;
    const float input[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float output[4] = {0.0f};
    if (!TelecoilFilter_Init(&filter, 16000U) ||
        !TelecoilFilter_ProcessBlock(&filter, input, output, 4U)) {
        return false;
    }
    TelecoilFilter_Reset(&filter);
    return (filter.stage[0].x1 == 0.0f) && (filter.stage[0].y1 == 0.0f) &&
           (filter.stage[1].x1 == 0.0f) && (filter.stage[1].y1 == 0.0f) &&
           TelecoilFilter_ProcessBlock(&filter, NULL, NULL, 0U) &&
           !TelecoilFilter_ProcessBlock(&filter, NULL, output, 1U);
}

static bool st_detector_to_controller_integration(void)
{
    TelecoilDetector_t detector;
    const TelecoilDetectorConfig_t detector_config = TelecoilDetector_DefaultConfig();
    TelecoilBlockMetrics_t metrics;
    int16_t strong[32];
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;

    for (size_t i = 0U; i < 32U; ++i) {
        strong[i] = ((i & 1U) == 0U) ? 2000 : -2000;
    }
    if ((TelecoilDetector_Init(&detector, &detector_config) != TELECOIL_DETECTOR_OK) ||
        !init_controller(&controller)) {
        return false;
    }
    (void)TelecoilDetector_Process(&detector, strong, 32U, &metrics);
    (void)TelecoilDetector_Process(&detector, strong, 32U, &metrics);
    inputs.now_ms = 100U;
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = metrics.valid_signal;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.active_source == AUDIO_SOURCE_TELECOIL);
}

static bool st_end_to_end_priority_sequence(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller)) {
        return false;
    }
    inputs.now_ms = 100U;
    inputs.interfaces_ready = true;
    inputs.telecoil_valid = true;
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        (outputs.active_source != AUDIO_SOURCE_TELECOIL)) {
        return false;
    }
    inputs.now_ms = 110U;
    inputs.bluetooth_connected = true;
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        (outputs.active_source != AUDIO_SOURCE_BLUETOOTH)) {
        return false;
    }
    inputs.now_ms = 120U;
    inputs.bluetooth_connected = false;
    if ((Connectivity_Update(&controller, &inputs, &outputs) != CONNECTIVITY_OK) ||
        (outputs.mode != CONNECTIVITY_M4_FAULT) || !outputs.muted) {
        return false;
    }
    inputs.now_ms = 220U;
    (void)Connectivity_Update(&controller, &inputs, &outputs);
    inputs.now_ms = 320U;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (outputs.active_source == AUDIO_SOURCE_TELECOIL);
}


static bool st_requirement_constants(void)
{
    return (REQ_STARTUP_TIMEOUT_MS == 3000U) &&
           (REQ_BT_CONNECT_TIMEOUT_MS == 5000U) &&
           (REQ_SOURCE_SWITCH_MAX_MS == 100U) &&
           (REQ_BT_AUDIO_LATENCY_MAX_MS == 100U) &&
           (REQ_BT_RECONNECT_PERIOD_MS == 2000U) &&
           (REQ_FAULT_RECOVERY_MAX_MS == 2000U) &&
           (REQ_FAULT_TO_IDLE_TIMEOUT_MS == 5000U) &&
           (REQ_FAULT_MUTE_MAX_MS == 50U) &&
           (REQ_TELECOIL_MIN_SNR_DB == 20U);
}

static bool st_signal_quality_20db_boundary(void)
{
    SignalQualityClass_t quality = SIGNAL_QUALITY_UNKNOWN;
    if (SignalQuality_Classify20dB(1000U, 100U, &quality) != SIGNAL_QUALITY_OK ||
        quality != SIGNAL_QUALITY_MEETS_20DB) {
        return false;
    }
    return (SignalQuality_Classify20dB(999U, 100U, &quality) == SIGNAL_QUALITY_OK) &&
           (quality == SIGNAL_QUALITY_BELOW_20DB);
}

static bool st_signal_quality_fixed_point_ratio(void)
{
    uint32_t ratio_q8 = 0U;
    return (SignalQuality_AmplitudeRatioQ8(1000U, 100U, &ratio_q8) == SIGNAL_QUALITY_OK) &&
           (ratio_q8 == 2560U);
}

static bool st_service_init_and_idle(void)
{
    ConnectivityService_t service;
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, false, false);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityOutputs_t outputs;
    return (ConnectivityService_GetStatus(&service, &outputs) == CONNECTIVITY_SERVICE_OK) &&
           (outputs.mode == CONNECTIVITY_M3_IDLE) && outputs.muted;
}

static bool st_service_monitors_telecoil_from_idle(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U, 2000);
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, false, false);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    if (ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U) !=
        CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    if (ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 3U) !=
        CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityOutputs_t outputs;
    return (ConnectivityService_GetStatus(&service, &outputs) == CONNECTIVITY_SERVICE_OK) &&
           (outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (outputs.active_source == AUDIO_SOURCE_TELECOIL) && !outputs.muted;
}

static bool st_service_bt_stale_boundary(void)
{
    ConnectivityService_t service;
    const int16_t input[1] = {1000};
    int16_t output[1] = {0};
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, true, true);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    if ((ConnectivityService_ProcessBluetoothBlock(&service, input, output, 1U, 10U, 110U) !=
         CONNECTIVITY_SERVICE_OK) || (output[0] != 1000)) {
        return false;
    }
    output[0] = 123;
    return (ConnectivityService_ProcessBluetoothBlock(&service, input, output, 1U, 10U, 111U) ==
            CONNECTIVITY_SERVICE_STALE_BT_FRAME) && (output[0] == 0);
}

static bool st_service_control_falls_back_to_telecoil(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U, 2000);
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, true, true);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    (void)ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U);
    (void)ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 3U);
    if (ConnectivityService_ApplyCommand(&service, CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH, 4U) !=
        CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityOutputs_t outputs;
    (void)ConnectivityService_GetStatus(&service, &outputs);
    return (outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (outputs.active_source == AUDIO_SOURCE_TELECOIL) &&
           (outputs.fault == CONNECTIVITY_FAULT_NONE);
}

static bool st_service_fault_recovery_diagnostics(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U, 2000);
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, true, true);
    (void)ConnectivityService_Step(&service, &state);
    state = service_link(2U, true, true, false);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK ||
        service.last_outputs.mode != CONNECTIVITY_M4_FAULT) {
        return false;
    }
    (void)ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 3U);
    (void)ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 4U);
    state = service_link(104U, true, true, false);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    return (service.last_outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
           (service.last_outputs.active_source == AUDIO_SOURCE_TELECOIL) &&
           (service.diagnostics.faults_entered == 1U) &&
           (service.diagnostics.recoveries == 1U);
}

static bool st_service_noise_quality(void)
{
    ConnectivityService_t service;
    int16_t input[32];
    int16_t output[32];
    fill_strong_telecoil(input, 32U, 2000);
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK ||
        ConnectivityService_SetTelecoilNoiseFloor(&service, 200U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, false, false);
    (void)ConnectivityService_Step(&service, &state);
    (void)ConnectivityService_ProcessTelecoilBlock(&service, input, output, 32U, 2U);
    return service.telecoil_quality == SIGNAL_QUALITY_MEETS_20DB;
}

static bool st_service_wrap_safe_bt_age(void)
{
    ConnectivityService_t service;
    const uint32_t start = UINT32_MAX - 20U;
    const int16_t input[1] = {1000};
    int16_t output[1] = {0};
    if (ConnectivityService_Init(&service, 48000U, start) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(UINT32_MAX - 10U, true, true, true);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    return (ConnectivityService_ProcessBluetoothBlock(&service,
                                                      input,
                                                      output,
                                                      1U,
                                                      UINT32_MAX - 5U,
                                                      20U) == CONNECTIVITY_SERVICE_OK) &&
           (output[0] == 1000);
}


static bool st_control_protocol_bt_off(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    return (ControlProtocol_Parse("bt off\r\n", 8U, &command) == CONTROL_PROTOCOL_OK) &&
           (command == CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH);
}

static bool st_control_protocol_status(void)
{
    return ControlProtocol_Parse(" STATUS ", 8U, NULL) == CONTROL_PROTOCOL_STATUS_REQUEST;
}

static bool st_control_protocol_rejects_malformed(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    return ControlProtocol_Parse("BT MAYBE", 8U, &command) == CONTROL_PROTOCOL_ERR_FORMAT;
}

static bool st_control_protocol_mode_bt(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    return (ControlProtocol_Parse("MODE BT", 7U, &command) == CONTROL_PROTOCOL_OK) &&
           (command == CONNECTIVITY_COMMAND_BLUETOOTH_ONLY);
}

static bool st_control_protocol_mode_tc(void)
{
    ConnectivityCommand_t command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
    return (ControlProtocol_Parse("MODE TC", 7U, &command) == CONTROL_PROTOCOL_OK) &&
           (command == CONNECTIVITY_COMMAND_TELECOIL_ONLY);
}

static bool st_idle_requests_low_power(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller)) {
        return false;
    }
    inputs.now_ms = 1U;
    inputs.interfaces_ready = true;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M3_IDLE) && outputs.request_low_power;
}

static bool st_active_does_not_request_low_power(void)
{
    ConnectivityController_t controller;
    ConnectivityInputs_t inputs = {0};
    ConnectivityOutputs_t outputs;
    if (!init_controller(&controller)) {
        return false;
    }
    inputs.now_ms = 1U;
    inputs.interfaces_ready = true;
    inputs.bluetooth_connected = true;
    return (Connectivity_Update(&controller, &inputs, &outputs) == CONNECTIVITY_OK) &&
           (outputs.mode == CONNECTIVITY_M2_ACTIVE) && !outputs.request_low_power;
}



typedef struct {
    uint32_t calls;
    bool allow;
} ActionSelfMock_t;

static bool self_action(void *context)
{
    ActionSelfMock_t *mock = (ActionSelfMock_t *)context;
    if (mock == NULL) {
        return false;
    }
    ++mock->calls;
    return mock->allow;
}

static bool st_pcm_transport_round_trip_shapes(void)
{
    const uint32_t words[4] = {0x000100U, 0x000300U, 0xFFFF00U, 0xFFFD00U};
    int16_t mono[2] = {0};
    int16_t stereo[4] = {0};
    return (PcmTransport_DecodeStereoWordsToMono(words, 2U, mono) == PCM_TRANSPORT_OK) &&
           (mono[0] == 2) && (mono[1] == -2) &&
           (PcmTransport_MonoToStereoS16(mono, 2U, stereo) == PCM_TRANSPORT_OK) &&
           (stereo[0] == 2) && (stereo[1] == 2) &&
           (stereo[2] == -2) && (stereo[3] == -2);
}

static bool st_pcm_transport_packed_s24(void)
{
    const uint8_t bytes[6] = {0x00U, 0x01U, 0x00U, 0x00U, 0xFFU, 0xFFU};
    int16_t output[2] = {0};
    return (PcmTransport_DecodePackedS24Le(bytes, 2U, output) == PCM_TRANSPORT_OK) &&
           (output[0] == 1) && (output[1] == -1);
}

static bool st_audio_dynamics_basic(void)
{
    AudioDynamics_t state;
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    const int16_t input[4] = {1000, -1000, 2000, -2000};
    int16_t output[4] = {0};
    return (AudioDynamics_Init(&state, &config) == AUDIO_DYNAMICS_OK) &&
           (AudioDynamics_ProcessBlock(&state, input, output, 4U, false) == AUDIO_DYNAMICS_OK) &&
           (output[0] != 0) && (output[3] != 0);
}

static bool st_audio_dynamics_mute(void)
{
    AudioDynamics_t state;
    const AudioDynamicsConfig_t config = AudioDynamics_DefaultConfig();
    const int16_t input[2] = {1000, -1000};
    int16_t output[2] = {1, 1};
    return (AudioDynamics_Init(&state, &config) == AUDIO_DYNAMICS_OK) &&
           (AudioDynamics_ProcessBlock(&state, input, output, 2U, true) == AUDIO_DYNAMICS_OK) &&
           (output[0] == 0) && (output[1] == 0);
}

static bool st_connectivity_actions_mock(void)
{
    ConnectivityActions_t actions;
    ConnectivityOutputs_t outputs = {0};
    ConnectivityActionPort_t port = {self_action, self_action, self_action, self_action};
    ActionSelfMock_t mock = {0U, true};
    outputs.request_bt_connect = true;
    outputs.request_low_power = true;
    return (ConnectivityActions_Init(&actions) == CONNECTIVITY_ACTIONS_OK) &&
           (ConnectivityActions_Apply(&actions, &outputs, &port, &mock) == CONNECTIVITY_ACTIONS_OK) &&
           (mock.calls == 2U) && actions.low_power_engaged;
}

static bool st_service_deadline_monitor(void)
{
    ConnectivityService_t service;
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(DESIGN_SERVICE_INTERVAL_MAX_MS + 1U,
                                                  true, false, false);
    return (ConnectivityService_Step(&service, &state) == CONNECTIVITY_SERVICE_OK) &&
           (service.diagnostics.service_deadline_misses == 1U);
}

static bool st_telecoil_loss_time_qualified(void)
{
    ConnectivityService_t service;
    int16_t strong[16];
    int16_t silent[16] = {0};
    int16_t output[16];
    fill_strong_telecoil(strong, 16U, 2000);
    if (ConnectivityService_Init(&service, 48000U, 0U) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    ConnectivityLinkState_t state = service_link(1U, true, false, false);
    if (ConnectivityService_Step(&service, &state) != CONNECTIVITY_SERVICE_OK ||
        ConnectivityService_ProcessTelecoilBlock(&service, strong, output, 16U, 2U) != CONNECTIVITY_SERVICE_OK ||
        ConnectivityService_ProcessTelecoilBlock(&service, strong, output, 16U, 3U) != CONNECTIVITY_SERVICE_OK ||
        !service.telecoil_valid_timed ||
        ConnectivityService_ProcessTelecoilBlock(&service, silent, output, 16U, 10U) != CONNECTIVITY_SERVICE_OK ||
        ConnectivityService_ProcessTelecoilBlock(&service, silent, output, 16U,
                                                  10U + DESIGN_TELECOIL_LOSS_CONFIRM_MS) != CONNECTIVITY_SERVICE_OK) {
        return false;
    }
    return (service.last_outputs.mode == CONNECTIVITY_M4_FAULT) && service.last_outputs.muted;
}

static bool st_timing_contract_is_inside_requirement(void)
{
    return DESIGN_TELECOIL_FAULT_WORST_CASE_MS <= REQ_SOURCE_SWITCH_MAX_MS;
}

static const SelfTestCase_t k_cases[] = {
    {"Audio sample-rate policy", st_sample_rates},
    {"PCM 24-to-16 extremes", st_pcm_decode_extremes},
    {"Gain rounding and saturation", st_gain_and_saturation},
    {"Stereo fold and mute", st_mix_and_mute},
    {"Audio block processing", st_audio_block},
    {"BT401 command table", st_bt_command_table},
    {"BT401 profile success mock", st_bt_profile_success},
    {"BT401 transport failure mock", st_bt_profile_failure},
    {"Connectivity initial state", st_connectivity_init},
    {"Telecoil active selection", st_telecoil_becomes_active},
    {"Bluetooth priority arbitration", st_bluetooth_priority},
    {"Bluetooth link-loss fault and mute", st_bt_link_loss_fault_and_mute},
    {"Reconnect exact 2 s boundary", st_reconnect_exact_boundary},
    {"Fault recovery stability", st_recovery_stability},
    {"Fault to idle exact 5 s boundary", st_fault_to_idle_timeout},
    {"Startup exact 3 s timeout", st_startup_timeout},
    {"BT exact 5 s connection timeout", st_bt_connection_timeout},
    {"Intentional disable is not a fault", st_intentional_disable_not_fault},
    {"32-bit timer rollover", st_timer_rollover},
    {"Telecoil silence rejection", st_detector_silence},
    {"Telecoil detector hysteresis", st_detector_hysteresis},
    {"Telecoil clipping rejection", st_detector_clipping_rejected},
    {"Telecoil filter sample-rate support", st_filter_supported_rates},
    {"Telecoil filter impulse stability", st_filter_impulse_stable},
    {"Telecoil filter block/reset safety", st_filter_block_and_reset},
    {"Detector-to-controller integration", st_detector_to_controller_integration},
    {"End-to-end source/fault/recovery sequence", st_end_to_end_priority_sequence},
    {"Requirement constants traceability", st_requirement_constants},
    {"Signal-quality exact 20 dB boundary", st_signal_quality_20db_boundary},
    {"Signal-quality Q8 ratio", st_signal_quality_fixed_point_ratio},
    {"Top-level service idle path", st_service_init_and_idle},
    {"Top-level service telecoil monitoring", st_service_monitors_telecoil_from_idle},
    {"Bluetooth 100 ms stale-frame guard", st_service_bt_stale_boundary},
    {"Control-command telecoil fallback", st_service_control_falls_back_to_telecoil},
    {"Service fault/recovery diagnostics", st_service_fault_recovery_diagnostics},
    {"Telecoil 20 dB quality integration", st_service_noise_quality},
    {"Service Bluetooth-age timer rollover", st_service_wrap_safe_bt_age},
    {"Control protocol BT OFF decode", st_control_protocol_bt_off},
    {"Control protocol STATUS decode", st_control_protocol_status},
    {"Control protocol malformed-frame rejection", st_control_protocol_rejects_malformed},
    {"Control protocol MODE BT decode", st_control_protocol_mode_bt},
    {"Control protocol MODE TC decode", st_control_protocol_mode_tc},
    {"Idle-mode low-power request", st_idle_requests_low_power},
    {"Active-mode low-power suppression", st_active_does_not_request_low_power},
    {"PCM transport stereo/mono path", st_pcm_transport_round_trip_shapes},
    {"PCM transport packed S24 decode", st_pcm_transport_packed_s24},
    {"Audio dynamics processing", st_audio_dynamics_basic},
    {"Audio dynamics mute", st_audio_dynamics_mute},
    {"Injected connectivity actions", st_connectivity_actions_mock},
    {"Service deadline monitor", st_service_deadline_monitor},
    {"Time-qualified telecoil loss", st_telecoil_loss_time_qualified},
    {"Timing contract within requirement", st_timing_contract_is_inside_requirement}
};

/** @copydoc SelfTest_RunAll */
int SelfTest_RunAll(SelfTestOutputFn output_fn)
{
    int failures = 0;
    g_output = output_fn;
    out("\r\n=== Independent framework-free self-test ===\r\n");

    for (size_t i = 0U; i < (sizeof(k_cases) / sizeof(k_cases[0])); ++i) {
        out("[SELFTEST] ");
        out(k_cases[i].name);
        if (k_cases[i].function()) {
            out(" : PASS\r\n");
        } else {
            out(" : FAIL\r\n");
            ++failures;
        }
    }

    if (failures == 0) {
        out("SELFTEST RESULT: ");
        out_uint(sizeof(k_cases) / sizeof(k_cases[0]));
        out(" checks, 0 failures, OK\r\n");
    } else {
        out("SELFTEST RESULT: FAILURES DETECTED\r\n");
    }
    return failures;
}
