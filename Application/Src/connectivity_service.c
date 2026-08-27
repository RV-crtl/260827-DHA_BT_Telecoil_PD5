/**
 * @file connectivity_service.c
 * @brief Implementation of the top-level portable Bluetooth/telecoil application service.
 */
#include "connectivity_service.h"

#include "audio_processing.h"
#include "requirements.h"

#include <limits.h>

/**
 * @brief Calculate elapsed 32-bit tick time using wrap-safe unsigned subtraction.
 * @param now_ms Current millisecond timestamp.
 * @param then_ms Earlier timestamp.
 * @return Elapsed milliseconds modulo 2^32.
 */
static uint32_t elapsed_ms(uint32_t now_ms, uint32_t then_ms)
{
    return now_ms - then_ms;
}

/**
 * @brief Reject implausibly older timestamps while allowing natural uint32_t rollover.
 * @param now_ms Candidate current timestamp.
 * @param previous_ms Previously accepted timestamp.
 * @return true when forward/equal under the less-than-2^31-ms call-spacing assumption.
 */
static bool time_is_forward_or_equal(uint32_t now_ms, uint32_t previous_ms)
{
    return elapsed_ms(now_ms, previous_ms) < UINT32_C(0x80000000);
}

/**
 * @brief Saturate a floating-point filter output into the S16 PCM range.
 * @param value Filtered sample.
 * @return Saturated signed 16-bit sample.
 */
static int16_t float_to_i16(float value)
{
    if (value >= 32767.0f) {
        return INT16_MAX;
    }
    if (value <= -32768.0f) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

/**
 * @brief Force a complete output block to digital silence.
 * @param output Valid output buffer.
 * @param sample_count Number of samples to clear.
 */
static void zero_block(int16_t *output, size_t sample_count)
{
    for (size_t i = 0U; i < sample_count; ++i) {
        output[i] = 0;
    }
}

/**
 * @brief Record integration cadence and count violations of the 40 ms service contract.
 * @param service Service whose diagnostics are updated.
 * @param now_ms Current accepted service timestamp.
 */
static void record_service_interval(ConnectivityService_t *service, uint32_t now_ms)
{
    const uint32_t interval = elapsed_ms(now_ms, service->last_service_ms);
    if (interval > service->diagnostics.max_service_interval_ms) {
        service->diagnostics.max_service_interval_ms = interval;
    }
    if (interval > DESIGN_SERVICE_INTERVAL_MAX_MS) {
        ++service->diagnostics.service_deadline_misses;
    }
    service->last_service_ms = now_ms;
}

/**
 * @brief Update counters that are derived from one controller transition.
 * @param service Service whose diagnostics are updated.
 * @param before Controller outputs before the step.
 * @param after Controller outputs after the step.
 */
static void update_diagnostics_from_transition(ConnectivityService_t *service,
                                               const ConnectivityOutputs_t *before,
                                               const ConnectivityOutputs_t *after)
{
    if (before->active_source != after->active_source) {
        ++service->diagnostics.source_switches;
    }
    if ((before->mode != CONNECTIVITY_M4_FAULT) &&
        (after->mode == CONNECTIVITY_M4_FAULT)) {
        ++service->diagnostics.faults_entered;
    }
    if ((before->mode == CONNECTIVITY_M4_FAULT) &&
        (after->mode == CONNECTIVITY_M2_ACTIVE)) {
        ++service->diagnostics.recoveries;
    }
    if (after->request_bt_connect) {
        ++service->diagnostics.bt_connect_requests;
    }
    if (after->request_bt_reconnect) {
        ++service->diagnostics.bt_reconnect_requests;
    }
}

/**
 * @brief Convert raw detector evidence into time-qualified telecoil validity.
 *
 * Activation still requires the detector's consecutive strong blocks. Once valid, a
 * clearly absent candidate starts a millisecond timer; validity is cleared only after
 * DESIGN_TELECOIL_LOSS_CONFIRM_MS. This makes loss timing independent of block size.
 *
 * @param service Service containing detector and timing state.
 * @param metrics Metrics for the just-processed block; validity fields are updated in place.
 * @param now_ms Current millisecond timestamp.
 */
static void update_time_qualified_telecoil(ConnectivityService_t *service,
                                           TelecoilBlockMetrics_t *metrics,
                                           uint32_t now_ms)
{
    const bool before = service->telecoil_valid_timed;

    if (metrics->raw_candidate_present) {
        service->telecoil_absence_tracking = false;
        /* Preserve the detector's two-strong-block activation hysteresis. */
        if (service->telecoil_detector.valid_signal) {
            service->telecoil_valid_timed = true;
        }
    } else if (service->telecoil_valid_timed) {
        if (!service->telecoil_absence_tracking) {
            service->telecoil_absence_tracking = true;
            service->telecoil_absent_since_ms = now_ms;
        } else if (elapsed_ms(now_ms, service->telecoil_absent_since_ms) >=
                   DESIGN_TELECOIL_LOSS_CONFIRM_MS) {
            service->telecoil_valid_timed = false;
            service->telecoil_absence_tracking = false;
        }
    } else {
        service->telecoil_absence_tracking = false;
    }

    metrics->valid_signal = service->telecoil_valid_timed;
    metrics->state_changed = (before != service->telecoil_valid_timed);
    if (metrics->state_changed) {
        ++service->diagnostics.telecoil_valid_transitions;
    }
}

/**
 * @brief Build controller inputs from stored service observations and execute one FSM step.
 * @param service Initialised service instance.
 * @param now_ms Current millisecond timestamp.
 * @return CONNECTIVITY_SERVICE_OK or a translated controller-state error.
 */
static ConnectivityServiceResult_t step_controller(ConnectivityService_t *service,
                                                    uint32_t now_ms)
{
    ConnectivityOutputs_t before;
    Connectivity_GetOutputs(&service->controller, &before);

    ConnectivityInputs_t inputs;
    inputs.now_ms = now_ms;
    inputs.interfaces_ready = service->link_state.interfaces_ready;
    inputs.paired_device_available = service->link_state.paired_device_available;
    inputs.bluetooth_connected = service->link_state.bluetooth_connected;
    inputs.telecoil_valid = service->telecoil_valid_timed;

    ConnectivityOutputs_t after;
    const ConnectivityResult_t result = Connectivity_Update(&service->controller, &inputs, &after);
    if (result != CONNECTIVITY_OK) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    ++service->diagnostics.controller_updates;
    update_diagnostics_from_transition(service, &before, &after);
    service->last_outputs = after;
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_Init */
ConnectivityServiceResult_t ConnectivityService_Init(ConnectivityService_t *service,
                                                      uint32_t sample_rate_hz,
                                                      uint32_t now_ms)
{
    if (service == NULL) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!Audio_IsSupportedSampleRate(sample_rate_hz)) {
        return CONNECTIVITY_SERVICE_ERR_SAMPLE_RATE;
    }

    *service = (ConnectivityService_t){0};
    ConnectivityConfig_t controller_config = Connectivity_DefaultConfig();
    if (Connectivity_Init(&service->controller, &controller_config, now_ms) != CONNECTIVITY_OK) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    const TelecoilDetectorConfig_t detector_config = TelecoilDetector_DefaultConfig();
    if (TelecoilDetector_Init(&service->telecoil_detector, &detector_config) != TELECOIL_DETECTOR_OK) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    if (!TelecoilFilter_Init(&service->telecoil_filter, sample_rate_hz)) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    service->link_state.now_ms = now_ms;
    service->last_service_ms = now_ms;
    service->sample_rate_hz = sample_rate_hz;
    service->bluetooth_gain_q15 = 32768;
    service->telecoil_gain_q15 = 32768;
    service->telecoil_quality = SIGNAL_QUALITY_UNKNOWN;
    service->initialised = true;
    Connectivity_GetOutputs(&service->controller, &service->last_outputs);
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_Step */
ConnectivityServiceResult_t ConnectivityService_Step(ConnectivityService_t *service,
                                                      const ConnectivityLinkState_t *link_state)
{
    if ((service == NULL) || (link_state == NULL)) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised ||
        !time_is_forward_or_equal(link_state->now_ms, service->link_state.now_ms)) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    record_service_interval(service, link_state->now_ms);
    service->link_state = *link_state;
    return step_controller(service, link_state->now_ms);
}

/** @copydoc ConnectivityService_ApplyCommand */
ConnectivityServiceResult_t ConnectivityService_ApplyCommand(ConnectivityService_t *service,
                                                              ConnectivityCommand_t command,
                                                              uint32_t now_ms)
{
    if (service == NULL) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised || !time_is_forward_or_equal(now_ms, service->link_state.now_ms)) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    bool bluetooth_enabled = service->controller.bluetooth_enabled;
    bool telecoil_enabled = service->controller.telecoil_enabled;
    switch (command) {
    case CONNECTIVITY_COMMAND_ENABLE_BLUETOOTH: bluetooth_enabled = true; break;
    case CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH: bluetooth_enabled = false; break;
    case CONNECTIVITY_COMMAND_ENABLE_TELECOIL: telecoil_enabled = true; break;
    case CONNECTIVITY_COMMAND_DISABLE_TELECOIL: telecoil_enabled = false; break;
    case CONNECTIVITY_COMMAND_ENABLE_BOTH: bluetooth_enabled = true; telecoil_enabled = true; break;
    case CONNECTIVITY_COMMAND_DISABLE_BOTH: bluetooth_enabled = false; telecoil_enabled = false; break;
    case CONNECTIVITY_COMMAND_BLUETOOTH_ONLY: bluetooth_enabled = true; telecoil_enabled = false; break;
    case CONNECTIVITY_COMMAND_TELECOIL_ONLY: bluetooth_enabled = false; telecoil_enabled = true; break;
    default: return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }

    const ConnectivityResult_t result = Connectivity_SetEnabled(&service->controller,
                                                                 bluetooth_enabled,
                                                                 telecoil_enabled,
                                                                 now_ms);
    if (result != CONNECTIVITY_OK) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    record_service_interval(service, now_ms);
    service->link_state.now_ms = now_ms;
    Connectivity_GetOutputs(&service->controller, &service->last_outputs);
    return step_controller(service, now_ms);
}

/** @copydoc ConnectivityService_SetGains */
ConnectivityServiceResult_t ConnectivityService_SetGains(ConnectivityService_t *service,
                                                          int32_t bluetooth_gain_q15,
                                                          int32_t telecoil_gain_q15)
{
    if (service == NULL) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    if ((bluetooth_gain_q15 < 0) || (telecoil_gain_q15 < 0)) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    service->bluetooth_gain_q15 = bluetooth_gain_q15;
    service->telecoil_gain_q15 = telecoil_gain_q15;
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_SetTelecoilNoiseFloor */
ConnectivityServiceResult_t ConnectivityService_SetTelecoilNoiseFloor(ConnectivityService_t *service,
                                                                       uint32_t noise_rms)
{
    if (service == NULL) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    service->telecoil_noise_rms = noise_rms;
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_ProcessTelecoilBlock */
ConnectivityServiceResult_t ConnectivityService_ProcessTelecoilBlock(ConnectivityService_t *service,
                                                                      const int16_t *input,
                                                                      int16_t *output,
                                                                      size_t sample_count,
                                                                      uint32_t now_ms)
{
    if ((service == NULL) || ((sample_count > 0U) && ((input == NULL) || (output == NULL))) ||
        (sample_count == 0U)) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised || !time_is_forward_or_equal(now_ms, service->link_state.now_ms)) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    record_service_interval(service, now_ms);
    TelecoilBlockMetrics_t metrics;
    if (TelecoilDetector_Process(&service->telecoil_detector, input, sample_count, &metrics) !=
        TELECOIL_DETECTOR_OK) {
        zero_block(output, sample_count);
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    ++service->diagnostics.telecoil_blocks_processed;
    update_time_qualified_telecoil(service, &metrics, now_ms);
    service->last_telecoil_metrics = metrics;

    if (service->telecoil_noise_rms == 0U) {
        service->telecoil_quality = SIGNAL_QUALITY_UNKNOWN;
    } else {
        (void)SignalQuality_Classify20dB(metrics.rms,
                                        service->telecoil_noise_rms,
                                        &service->telecoil_quality);
    }

    service->link_state.now_ms = now_ms;
    const ConnectivityServiceResult_t step_result = step_controller(service, now_ms);
    if (step_result != CONNECTIVITY_SERVICE_OK) {
        zero_block(output, sample_count);
        return step_result;
    }

    const bool route_audio = (service->last_outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
                             (service->last_outputs.active_source == AUDIO_SOURCE_TELECOIL) &&
                             !service->last_outputs.muted;
    for (size_t i = 0U; i < sample_count; ++i) {
        if (!route_audio) {
            output[i] = 0;
            continue;
        }
        const float filtered = TelecoilFilter_ProcessSample(&service->telecoil_filter, (float)input[i]);
        output[i] = Audio_ApplyGainQ15(float_to_i16(filtered), service->telecoil_gain_q15);
    }
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_ProcessBluetoothBlock */
ConnectivityServiceResult_t ConnectivityService_ProcessBluetoothBlock(ConnectivityService_t *service,
                                                                       const int16_t *input,
                                                                       int16_t *output,
                                                                       size_t sample_count,
                                                                       uint32_t received_ms,
                                                                       uint32_t now_ms)
{
    if ((service == NULL) || ((sample_count > 0U) && ((input == NULL) || (output == NULL))) ||
        (sample_count == 0U)) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised || !time_is_forward_or_equal(now_ms, service->link_state.now_ms) ||
        !time_is_forward_or_equal(now_ms, received_ms)) {
        zero_block(output, sample_count);
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }

    record_service_interval(service, now_ms);
    service->link_state.now_ms = now_ms;
    const ConnectivityServiceResult_t step_result = step_controller(service, now_ms);
    if (step_result != CONNECTIVITY_SERVICE_OK) {
        zero_block(output, sample_count);
        return step_result;
    }

    if (elapsed_ms(now_ms, received_ms) > REQ_BT_AUDIO_LATENCY_MAX_MS) {
        ++service->diagnostics.stale_bt_frames_dropped;
        zero_block(output, sample_count);
        return CONNECTIVITY_SERVICE_STALE_BT_FRAME;
    }

    const bool route_audio = (service->last_outputs.mode == CONNECTIVITY_M2_ACTIVE) &&
                             (service->last_outputs.active_source == AUDIO_SOURCE_BLUETOOTH) &&
                             !service->last_outputs.muted;
    for (size_t i = 0U; i < sample_count; ++i) {
        output[i] = route_audio ? Audio_ApplyGainQ15(input[i], service->bluetooth_gain_q15) : 0;
    }
    if (route_audio) {
        ++service->diagnostics.bt_frames_processed;
    }
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_GetStatus */
ConnectivityServiceResult_t ConnectivityService_GetStatus(const ConnectivityService_t *service,
                                                           ConnectivityOutputs_t *outputs)
{
    if ((service == NULL) || (outputs == NULL)) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    *outputs = service->last_outputs;
    return CONNECTIVITY_SERVICE_OK;
}

/** @copydoc ConnectivityService_ResetDiagnostics */
ConnectivityServiceResult_t ConnectivityService_ResetDiagnostics(ConnectivityService_t *service)
{
    if (service == NULL) {
        return CONNECTIVITY_SERVICE_ERR_ARGUMENT;
    }
    if (!service->initialised) {
        return CONNECTIVITY_SERVICE_ERR_STATE;
    }
    service->diagnostics = (ConnectivityDiagnostics_t){0};
    service->last_service_ms = service->link_state.now_ms;
    return CONNECTIVITY_SERVICE_OK;
}
