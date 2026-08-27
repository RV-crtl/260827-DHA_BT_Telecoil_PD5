/**
 * @file connectivity_actions.h
 * @brief Dependency-injected boundary between portable state decisions and external actions.
 *
 * `connectivity_controller` deliberately emits one-shot requests rather than touching
 * UARTs, power registers or an RTOS. This module consumes those requests through a small
 * callback port. A later hardware adapter can implement the callbacks, while unit tests
 * use mocks/spies to verify exactly which external action would have been requested.
 */
#ifndef CONNECTIVITY_ACTIONS_H
#define CONNECTIVITY_ACTIONS_H

#include "connectivity_controller.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic external-action callback.
 * @param context Caller-owned adapter or mock context.
 * @return true when the requested action was accepted successfully.
 */
typedef bool (*ConnectivityActionFn)(void *context);

/** Injected operations available to the portable connectivity layer. */
typedef struct {
    ConnectivityActionFn connect_bluetooth;   /**< Request an initial Bluetooth connection. */
    ConnectivityActionFn reconnect_bluetooth; /**< Request a Bluetooth reconnection attempt. */
    ConnectivityActionFn enter_low_power;     /**< Request entry to the platform low-power state. */
    ConnectivityActionFn exit_low_power;      /**< Request exit from the platform low-power state. */
} ConnectivityActionPort_t;

/** Result codes returned while dispatching external actions. */
typedef enum {
    CONNECTIVITY_ACTIONS_OK = 0,
    CONNECTIVITY_ACTIONS_ERR_ARGUMENT,
    CONNECTIVITY_ACTIONS_ERR_CALLBACK
} ConnectivityActionsResult_t;

/** Persistent action-dispatch state and lightweight diagnostics. */
typedef struct {
    bool low_power_engaged;     /**< Last successfully commanded low-power state. */
    uint32_t connect_calls;     /**< Successful initial-connect requests dispatched. */
    uint32_t reconnect_calls;   /**< Successful reconnect requests dispatched. */
    uint32_t low_power_entries; /**< Successful low-power entry requests dispatched. */
    uint32_t low_power_exits;   /**< Successful low-power exit requests dispatched. */
} ConnectivityActions_t;

/**
 * @brief Initialise the action-dispatch state to a known active-power condition.
 * @param state State object to initialise.
 * @return CONNECTIVITY_ACTIONS_OK on success, or an argument error for NULL.
 */
ConnectivityActionsResult_t ConnectivityActions_Init(ConnectivityActions_t *state);

/**
 * @brief Dispatch one-shot controller requests through the injected action port.
 *
 * Bluetooth connect/reconnect requests are forwarded when asserted. Low-power entry and
 * exit are edge-triggered so the callback is not repeatedly invoked while the requested
 * state remains unchanged. NULL individual callbacks are allowed and treated as no-op
 * platform capabilities; a callback that returns false causes an error immediately.
 *
 * @param state Initialised dispatch state.
 * @param outputs Current controller outputs containing request flags.
 * @param port Callback table representing the integration boundary.
 * @param context Caller-owned context forwarded to callbacks.
 * @return CONNECTIVITY_ACTIONS_OK, an argument error, or a callback failure.
 */
ConnectivityActionsResult_t ConnectivityActions_Apply(ConnectivityActions_t *state,
                                                       const ConnectivityOutputs_t *outputs,
                                                       const ConnectivityActionPort_t *port,
                                                       void *context);

#ifdef __cplusplus
}
#endif

#endif
