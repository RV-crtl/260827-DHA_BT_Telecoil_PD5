/**
 * @file connectivity_controller.c
 * @brief Implementation of the deterministic M1-M4 connectivity finite-state machine.
 */
#include "connectivity_controller.h"
#include "requirements.h"

#include <stddef.h>

/**
 * @brief Validate controller timing fields and their required relationships.
 * @param config Configuration to validate.
 * @return true when all mandatory intervals are non-zero and recovery debounce fits its limit.
 */
static bool config_valid(const ConnectivityConfig_t *config)
{
    return (config != NULL) &&
           (config->startup_timeout_ms > 0U) &&
           (config->bt_connect_timeout_ms > 0U) &&
           (config->bt_reconnect_period_ms > 0U) &&
           (config->recovery_limit_ms > 0U) &&
           (config->fault_to_idle_ms > 0U) &&
           (config->recovery_stable_ms <= config->recovery_limit_ms);
}

/**
 * @brief Calculate elapsed 32-bit tick time using wrap-safe unsigned subtraction.
 * @param now_ms Current timestamp.
 * @param then_ms Earlier timestamp.
 * @return Elapsed time modulo 2^32 milliseconds.
 */
static uint32_t elapsed_ms(uint32_t now_ms, uint32_t then_ms)
{
    /* Unsigned subtraction is intentionally wrap-safe for a 32-bit millisecond tick. */
    return now_ms - then_ms;
}

/**
 * @brief Reject stale timestamps while allowing a natural uint32_t millisecond wrap.
 * @param now_ms Candidate current timestamp.
 * @param previous_ms Previously accepted timestamp.
 * @return true for forward/equal time under the less-than-2^31-ms call-spacing assumption.
 */
static bool time_is_forward_or_equal(uint32_t now_ms, uint32_t previous_ms)
{
    /* Accept normal progression and one natural UINT32 wrap, while rejecting
     * a genuinely older timestamp. Calls are assumed to be less than 2^31 ms apart. */
    return elapsed_ms(now_ms, previous_ms) < 0x80000000UL;
}

/**
 * @brief Select the highest-priority source that is both enabled and currently valid.
 * @param controller Controller containing enable policy.
 * @param inputs Current source-validity observations.
 * @return Bluetooth when both are valid, otherwise Telecoil, otherwise None.
 */
static ConnectivityAudioSource_t best_valid_source(const ConnectivityController_t *controller,
                                                    const ConnectivityInputs_t *inputs)
{
    if (controller->bluetooth_enabled && inputs->bluetooth_connected) {
        return AUDIO_SOURCE_BLUETOOTH;
    }
    if (controller->telecoil_enabled && inputs->telecoil_valid) {
        return AUDIO_SOURCE_TELECOIL;
    }
    return AUDIO_SOURCE_NONE;
}

/**
 * @brief Change the selected audio source and timestamp the transition only when it changes.
 * @param controller Controller state to update.
 * @param source New source selection.
 * @param now_ms Timestamp associated with the transition.
 */
static void set_source(ConnectivityController_t *controller,
                       ConnectivityAudioSource_t source,
                       uint32_t now_ms)
{
    if (controller->active_source != source) {
        controller->active_source = source;
        controller->source_changed_ms = now_ms;
    }
}

/**
 * @brief Change M1-M4 mode and timestamp the transition only when it changes.
 * @param controller Controller state to update.
 * @param mode New system mode.
 * @param now_ms Timestamp associated with the transition.
 */
static void set_mode(ConnectivityController_t *controller,
                     ConnectivityMode_t mode,
                     uint32_t now_ms)
{
    if (controller->mode != mode) {
        controller->mode = mode;
        controller->mode_changed_ms = now_ms;
    }
}

/**
 * @brief Enter M2 Active with a valid selected source.
 * @param controller Controller state to update.
 * @param source Valid source to select.
 * @param now_ms Transition timestamp.
 */
static void enter_active(ConnectivityController_t *controller,
                         ConnectivityAudioSource_t source,
                         uint32_t now_ms)
{
    controller->fault = CONNECTIVITY_FAULT_NONE;
    controller->muted = false;
    controller->recovery_candidate = AUDIO_SOURCE_NONE;
    controller->recovery_candidate_since_ms = now_ms;
    set_source(controller, source, now_ms);
    set_mode(controller, CONNECTIVITY_M2_ACTIVE, now_ms);
}

/**
 * @brief Enter M3 Idle with no source selected and output muted.
 * @param controller Controller state to update.
 * @param now_ms Transition timestamp.
 */
static void enter_idle(ConnectivityController_t *controller, uint32_t now_ms)
{
    controller->fault = CONNECTIVITY_FAULT_NONE;
    controller->muted = true;
    controller->recovery_candidate = AUDIO_SOURCE_NONE;
    set_source(controller, AUDIO_SOURCE_NONE, now_ms);
    set_mode(controller, CONNECTIVITY_M3_IDLE, now_ms);
}

/**
 * @brief Enter M4 Fault, latch the reason, mute output and initialise recovery timers.
 * @param controller Controller state to update.
 * @param fault Fault reason to retain while in M4.
 * @param now_ms Fault-entry timestamp.
 */
static void enter_fault(ConnectivityController_t *controller,
                        ConnectivityFault_t fault,
                        uint32_t now_ms)
{
    controller->fault = fault;
    controller->muted = true;
    controller->fault_entered_ms = now_ms;
    controller->last_bt_reconnect_ms = now_ms;
    controller->recovery_candidate = AUDIO_SOURCE_NONE;
    controller->recovery_candidate_since_ms = now_ms;
    controller->no_valid_since_ms = now_ms;
    set_source(controller, AUDIO_SOURCE_NONE, now_ms);
    set_mode(controller, CONNECTIVITY_M4_FAULT, now_ms);
}

/**
 * @brief Copy persistent controller status into the public output structure.
 * @param controller Controller state to copy.
 * @param outputs Destination snapshot.
 */
static void snapshot(const ConnectivityController_t *controller,
                     ConnectivityOutputs_t *outputs)
{
    outputs->mode = controller->mode;
    outputs->active_source = controller->active_source;
    outputs->fault = controller->fault;
    outputs->muted = controller->muted;
    outputs->request_low_power = (controller->mode == CONNECTIVITY_M3_IDLE);
    outputs->source_changed_ms = controller->source_changed_ms;
    outputs->mode_changed_ms = controller->mode_changed_ms;
}

/** @copydoc Connectivity_DefaultConfig */
ConnectivityConfig_t Connectivity_DefaultConfig(void)
{
    ConnectivityConfig_t config;
    config.startup_timeout_ms = REQ_STARTUP_TIMEOUT_MS;
    config.bt_connect_timeout_ms = REQ_BT_CONNECT_TIMEOUT_MS;
    config.bt_reconnect_period_ms = REQ_BT_RECONNECT_PERIOD_MS;
    config.recovery_limit_ms = REQ_FAULT_RECOVERY_MAX_MS;
    config.fault_to_idle_ms = REQ_FAULT_TO_IDLE_TIMEOUT_MS;
    config.recovery_stable_ms = DESIGN_RECOVERY_STABLE_MS;
    return config;
}

/** @copydoc Connectivity_Init */
ConnectivityResult_t Connectivity_Init(ConnectivityController_t *controller,
                                       const ConnectivityConfig_t *config,
                                       uint32_t now_ms)
{
    if (controller == NULL) {
        return CONNECTIVITY_ERR_ARGUMENT;
    }
    if (!config_valid(config)) {
        return CONNECTIVITY_ERR_CONFIG;
    }

    *controller = (ConnectivityController_t){0};
    controller->config = *config;
    controller->mode = CONNECTIVITY_M1_INITIALISING;
    controller->active_source = AUDIO_SOURCE_NONE;
    controller->fault = CONNECTIVITY_FAULT_NONE;
    controller->muted = true;
    controller->bluetooth_enabled = true;
    controller->telecoil_enabled = true;
    controller->start_ms = now_ms;
    controller->last_update_ms = now_ms;
    controller->mode_changed_ms = now_ms;
    controller->source_changed_ms = now_ms;
    controller->fault_entered_ms = now_ms;
    controller->last_bt_reconnect_ms = now_ms;
    controller->recovery_candidate_since_ms = now_ms;
    controller->no_valid_since_ms = now_ms;
    return CONNECTIVITY_OK;
}

/** @copydoc Connectivity_SetEnabled */
ConnectivityResult_t Connectivity_SetEnabled(ConnectivityController_t *controller,
                                             bool bluetooth_enabled,
                                             bool telecoil_enabled,
                                             uint32_t now_ms)
{
    if (controller == NULL) {
        return CONNECTIVITY_ERR_ARGUMENT;
    }
    if (!time_is_forward_or_equal(now_ms, controller->last_update_ms)) {
        return CONNECTIVITY_ERR_TIME_BACKWARDS;
    }

    controller->bluetooth_enabled = bluetooth_enabled;
    controller->telecoil_enabled = telecoil_enabled;
    controller->last_update_ms = now_ms;

    if (!bluetooth_enabled && (controller->active_source == AUDIO_SOURCE_BLUETOOTH)) {
        controller->muted = true;
        set_source(controller, AUDIO_SOURCE_NONE, now_ms);
        set_mode(controller, CONNECTIVITY_M3_IDLE, now_ms);
    }
    if (!telecoil_enabled && (controller->active_source == AUDIO_SOURCE_TELECOIL)) {
        controller->muted = true;
        set_source(controller, AUDIO_SOURCE_NONE, now_ms);
        set_mode(controller, CONNECTIVITY_M3_IDLE, now_ms);
    }

    return CONNECTIVITY_OK;
}

/** @copydoc Connectivity_Update */
ConnectivityResult_t Connectivity_Update(ConnectivityController_t *controller,
                                         const ConnectivityInputs_t *inputs,
                                         ConnectivityOutputs_t *outputs)
{
    if ((controller == NULL) || (inputs == NULL) || (outputs == NULL)) {
        return CONNECTIVITY_ERR_ARGUMENT;
    }
    if (!time_is_forward_or_equal(inputs->now_ms, controller->last_update_ms)) {
        return CONNECTIVITY_ERR_TIME_BACKWARDS;
    }

    *outputs = (ConnectivityOutputs_t){0};
    const uint32_t now_ms = inputs->now_ms;

    switch (controller->mode) {
    case CONNECTIVITY_M1_INITIALISING:
        if (!inputs->interfaces_ready) {
            if (elapsed_ms(now_ms, controller->start_ms) >= controller->config.startup_timeout_ms) {
                enter_fault(controller, CONNECTIVITY_FAULT_STARTUP_TIMEOUT, now_ms);
            }
            break;
        }

        if (controller->bluetooth_enabled && inputs->bluetooth_connected) {
            enter_active(controller, AUDIO_SOURCE_BLUETOOTH, now_ms);
            break;
        }
        if (controller->telecoil_enabled && inputs->telecoil_valid) {
            enter_active(controller, AUDIO_SOURCE_TELECOIL, now_ms);
            break;
        }

        if (controller->bluetooth_enabled && inputs->paired_device_available) {
            outputs->request_bt_connect = true;
            if (elapsed_ms(now_ms, controller->start_ms) >= controller->config.bt_connect_timeout_ms) {
                enter_fault(controller, CONNECTIVITY_FAULT_BT_CONNECTION_TIMEOUT, now_ms);
            }
        } else {
            enter_idle(controller, now_ms);
        }
        break;

    case CONNECTIVITY_M2_ACTIVE:
        if (controller->active_source == AUDIO_SOURCE_BLUETOOTH) {
            if (!controller->bluetooth_enabled) {
                const ConnectivityAudioSource_t alternate = best_valid_source(controller, inputs);
                if (alternate != AUDIO_SOURCE_NONE) {
                    enter_active(controller, alternate, now_ms);
                } else {
                    enter_idle(controller, now_ms);
                }
            } else if (!inputs->bluetooth_connected) {
                enter_fault(controller, CONNECTIVITY_FAULT_BT_LINK_LOSS, now_ms);
            }
        } else if (controller->active_source == AUDIO_SOURCE_TELECOIL) {
            if (!controller->telecoil_enabled) {
                const ConnectivityAudioSource_t alternate = best_valid_source(controller, inputs);
                if (alternate != AUDIO_SOURCE_NONE) {
                    enter_active(controller, alternate, now_ms);
                } else {
                    enter_idle(controller, now_ms);
                }
            } else if (!inputs->telecoil_valid) {
                enter_fault(controller, CONNECTIVITY_FAULT_TELECOIL_INVALID, now_ms);
            } else if (controller->bluetooth_enabled && inputs->bluetooth_connected) {
                enter_active(controller, AUDIO_SOURCE_BLUETOOTH, now_ms);
            }
        } else {
            const ConnectivityAudioSource_t best = best_valid_source(controller, inputs);
            if (best != AUDIO_SOURCE_NONE) {
                enter_active(controller, best, now_ms);
            } else {
                enter_idle(controller, now_ms);
            }
        }
        break;

    case CONNECTIVITY_M3_IDLE: {
        const ConnectivityAudioSource_t best = best_valid_source(controller, inputs);
        if (best != AUDIO_SOURCE_NONE) {
            enter_active(controller, best, now_ms);
        }
        break;
    }

    case CONNECTIVITY_M4_FAULT: {
        const ConnectivityAudioSource_t best = best_valid_source(controller, inputs);

        if ((controller->fault == CONNECTIVITY_FAULT_BT_LINK_LOSS) ||
            (controller->fault == CONNECTIVITY_FAULT_BT_CONNECTION_TIMEOUT)) {
            if (controller->bluetooth_enabled && inputs->paired_device_available &&
                (elapsed_ms(now_ms, controller->last_bt_reconnect_ms) >=
                 controller->config.bt_reconnect_period_ms)) {
                outputs->request_bt_reconnect = true;
                controller->last_bt_reconnect_ms = now_ms;
            }
        }

        if (best != AUDIO_SOURCE_NONE) {
            controller->no_valid_since_ms = now_ms;
            if (best != controller->recovery_candidate) {
                controller->recovery_candidate = best;
                controller->recovery_candidate_since_ms = now_ms;
            } else if (elapsed_ms(now_ms, controller->recovery_candidate_since_ms) >=
                       controller->config.recovery_stable_ms) {
                enter_active(controller, best, now_ms);
            }
        } else {
            if (controller->recovery_candidate != AUDIO_SOURCE_NONE) {
                controller->no_valid_since_ms = now_ms;
            }
            controller->recovery_candidate = AUDIO_SOURCE_NONE;
            controller->recovery_candidate_since_ms = now_ms;
            if (elapsed_ms(now_ms, controller->no_valid_since_ms) >= controller->config.fault_to_idle_ms) {
                enter_idle(controller, now_ms);
            }
        }
        break;
    }

    default:
        return CONNECTIVITY_ERR_CONFIG;
    }

    controller->last_update_ms = now_ms;
    snapshot(controller, outputs);
    return CONNECTIVITY_OK;
}

/** @copydoc Connectivity_GetOutputs */
void Connectivity_GetOutputs(const ConnectivityController_t *controller,
                             ConnectivityOutputs_t *outputs)
{
    if ((controller == NULL) || (outputs == NULL)) {
        return;
    }
    *outputs = (ConnectivityOutputs_t){0};
    snapshot(controller, outputs);
}
