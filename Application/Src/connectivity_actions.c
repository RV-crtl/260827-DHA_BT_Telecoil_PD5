/**
 * @file connectivity_actions.c
 * @brief Implementation of the callback-based external-action dispatcher.
 */
#include "connectivity_actions.h"

#include <stddef.h>

/**
 * @brief Invoke one optional integration callback and translate its Boolean result.
 * @param function Callback to invoke; NULL is treated as a supported no-op.
 * @param context Caller-owned callback context.
 * @return CONNECTIVITY_ACTIONS_OK or CONNECTIVITY_ACTIONS_ERR_CALLBACK.
 */
static ConnectivityActionsResult_t invoke(ConnectivityActionFn function,
                                          void *context)
{
    if (function == NULL) {
        return CONNECTIVITY_ACTIONS_OK;
    }
    return function(context) ? CONNECTIVITY_ACTIONS_OK : CONNECTIVITY_ACTIONS_ERR_CALLBACK;
}

/** @copydoc ConnectivityActions_Init */
ConnectivityActionsResult_t ConnectivityActions_Init(ConnectivityActions_t *state)
{
    if (state == NULL) {
        return CONNECTIVITY_ACTIONS_ERR_ARGUMENT;
    }
    *state = (ConnectivityActions_t){0};
    return CONNECTIVITY_ACTIONS_OK;
}

/** @copydoc ConnectivityActions_Apply */
ConnectivityActionsResult_t ConnectivityActions_Apply(ConnectivityActions_t *state,
                                                       const ConnectivityOutputs_t *outputs,
                                                       const ConnectivityActionPort_t *port,
                                                       void *context)
{
    if ((state == NULL) || (outputs == NULL) || (port == NULL)) {
        return CONNECTIVITY_ACTIONS_ERR_ARGUMENT;
    }

    if (outputs->request_bt_connect) {
        if (invoke(port->connect_bluetooth, context) != CONNECTIVITY_ACTIONS_OK) {
            return CONNECTIVITY_ACTIONS_ERR_CALLBACK;
        }
        ++state->connect_calls;
    }
    if (outputs->request_bt_reconnect) {
        if (invoke(port->reconnect_bluetooth, context) != CONNECTIVITY_ACTIONS_OK) {
            return CONNECTIVITY_ACTIONS_ERR_CALLBACK;
        }
        ++state->reconnect_calls;
    }

    if (outputs->request_low_power && !state->low_power_engaged) {
        if (invoke(port->enter_low_power, context) != CONNECTIVITY_ACTIONS_OK) {
            return CONNECTIVITY_ACTIONS_ERR_CALLBACK;
        }
        state->low_power_engaged = true;
        ++state->low_power_entries;
    } else if (!outputs->request_low_power && state->low_power_engaged) {
        if (invoke(port->exit_low_power, context) != CONNECTIVITY_ACTIONS_OK) {
            return CONNECTIVITY_ACTIONS_ERR_CALLBACK;
        }
        state->low_power_engaged = false;
        ++state->low_power_exits;
    }

    return CONNECTIVITY_ACTIONS_OK;
}
