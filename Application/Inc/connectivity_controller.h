/**
 * @file connectivity_controller.h
 * @brief Deterministic M1-M4 Bluetooth/telecoil finite-state machine.
 *
 * The controller contains the hardware-independent policy for startup, source arbitration,
 * fault handling, Bluetooth reconnect requests, stable recovery and idle entry. System mode
 * and audio-source identity are deliberately separate concepts: M2 means "active", while
 * the selected source is independently reported as Bluetooth or telecoil.
 *
 * All elapsed-time calculations use unsigned 32-bit subtraction, which naturally handles
 * one `uint32_t` timer wrap. The public update API also rejects implausibly older timestamps;
 * calls are assumed to be separated by less than 2^31 ms.
 */
#ifndef CONNECTIVITY_CONTROLLER_H
#define CONNECTIVITY_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes returned by connectivity-controller operations. */
typedef enum {
    CONNECTIVITY_OK = 0,
    CONNECTIVITY_ERR_ARGUMENT,
    CONNECTIVITY_ERR_CONFIG,
    CONNECTIVITY_ERR_TIME_BACKWARDS
} ConnectivityResult_t;

/** System modes defined for the Bluetooth/telecoil subsystem. */
typedef enum {
    CONNECTIVITY_M1_INITIALISING = 1, /**< Startup/interface initialisation. */
    CONNECTIVITY_M2_ACTIVE = 2,       /**< Valid audio source selected and usable. */
    CONNECTIVITY_M3_IDLE = 3,         /**< No active source; low-power operation may be requested. */
    CONNECTIVITY_M4_FAULT = 4         /**< Invalid/failed active source; output muted. */
} ConnectivityMode_t;

/** Audio source selected by the deterministic priority arbiter. */
typedef enum {
    AUDIO_SOURCE_NONE = 0,
    AUDIO_SOURCE_TELECOIL,
    AUDIO_SOURCE_BLUETOOTH
} ConnectivityAudioSource_t;

/** Fault reason retained while the controller is in M4. */
typedef enum {
    CONNECTIVITY_FAULT_NONE = 0,
    CONNECTIVITY_FAULT_STARTUP_TIMEOUT,
    CONNECTIVITY_FAULT_BT_CONNECTION_TIMEOUT,
    CONNECTIVITY_FAULT_BT_LINK_LOSS,
    CONNECTIVITY_FAULT_TELECOIL_INVALID
} ConnectivityFault_t;

/** Timing policy used by the hardware-independent state machine. */
typedef struct {
    uint32_t startup_timeout_ms;      /**< Maximum M1 interface-initialisation time. */
    uint32_t bt_connect_timeout_ms;   /**< Maximum initial paired-device connection window. */
    uint32_t bt_reconnect_period_ms;  /**< Period between reconnect requests in M4. */
    uint32_t recovery_limit_ms;       /**< Maximum requirement budget available for recovery. */
    uint32_t fault_to_idle_ms;        /**< Time with no valid input before M4 -> M3. */
    uint32_t recovery_stable_ms;      /**< Continuous validity required before M4 -> M2. */
} ConnectivityConfig_t;

/** External observations supplied to one state-machine update. */
typedef struct {
    uint32_t now_ms;                  /**< Current caller-supplied millisecond timestamp. */
    bool interfaces_ready;            /**< Bluetooth/telecoil interfaces finished initialising. */
    bool paired_device_available;     /**< A previously paired Bluetooth device is available. */
    bool bluetooth_connected;         /**< Bluetooth audio link is currently valid. */
    bool telecoil_valid;              /**< Time-qualified telecoil signal is currently valid. */
} ConnectivityInputs_t;

/** Observable state and one-shot requests produced by one state-machine update. */
typedef struct {
    ConnectivityMode_t mode;                    /**< Current M1-M4 system mode. */
    ConnectivityAudioSource_t active_source;    /**< Currently selected audio source. */
    ConnectivityFault_t fault;                  /**< Latched M4 fault reason, or NONE. */
    bool muted;                                 /**< True when output must be silenced. */
    bool request_bt_connect;                    /**< One-shot initial Bluetooth-connect request. */
    bool request_bt_reconnect;                  /**< One-shot Bluetooth-reconnect request. */
    bool request_low_power;                     /**< True while M3 permits platform low-power entry. */
    uint32_t source_changed_ms;                 /**< Timestamp of the most recent source change. */
    uint32_t mode_changed_ms;                   /**< Timestamp of the most recent mode change. */
} ConnectivityOutputs_t;

/** Persistent state owned by one controller instance. */
typedef struct {
    ConnectivityConfig_t config;
    ConnectivityMode_t mode;
    ConnectivityAudioSource_t active_source;
    ConnectivityFault_t fault;
    bool muted;
    bool bluetooth_enabled;
    bool telecoil_enabled;
    uint32_t start_ms;
    uint32_t last_update_ms;
    uint32_t mode_changed_ms;
    uint32_t source_changed_ms;
    uint32_t fault_entered_ms;
    uint32_t last_bt_reconnect_ms;
    uint32_t recovery_candidate_since_ms;
    uint32_t no_valid_since_ms;
    ConnectivityAudioSource_t recovery_candidate;
} ConnectivityController_t;

/**
 * @brief Return the requirement-derived default timing policy.
 * @return Valid configuration containing startup, connection, reconnect, recovery and idle timings.
 */
ConnectivityConfig_t Connectivity_DefaultConfig(void);

/**
 * @brief Initialise a controller in M1 with Bluetooth and telecoil enabled.
 *
 * @param controller Controller instance to initialise.
 * @param config Valid timing configuration.
 * @param now_ms Initial 32-bit millisecond timestamp.
 * @return CONNECTIVITY_OK on success, or an argument/configuration error.
 */
ConnectivityResult_t Connectivity_Init(ConnectivityController_t *controller,
                                       const ConnectivityConfig_t *config,
                                       uint32_t now_ms);

/**
 * @brief Enable or disable sources without treating an intentional disable as a fault.
 *
 * If the currently selected source is deliberately disabled, the controller immediately
 * clears that source and moves to M3 rather than synthesising a link-loss/signal fault.
 *
 * @param controller Initialised controller.
 * @param bluetooth_enabled Desired Bluetooth enable state.
 * @param telecoil_enabled Desired telecoil enable state.
 * @param now_ms Current millisecond timestamp.
 * @return CONNECTIVITY_OK or an argument/backwards-time error.
 */
ConnectivityResult_t Connectivity_SetEnabled(ConnectivityController_t *controller,
                                             bool bluetooth_enabled,
                                             bool telecoil_enabled,
                                             uint32_t now_ms);

/**
 * @brief Advance the finite-state machine from one snapshot of external observations.
 *
 * Bluetooth has deterministic priority whenever both sources are valid. Event outputs
 * (`request_bt_connect` and `request_bt_reconnect`) apply only to the current update.
 * Active-source loss enters M4 and asserts mute in the same update. A returning source
 * must remain stable for the configured recovery interval before M4 clears.
 *
 * @param controller Initialised controller whose persistent state is updated.
 * @param inputs Current external observations and timestamp.
 * @param outputs Receives the resulting state and one-shot request flags.
 * @return CONNECTIVITY_OK, or an argument/backwards-time error.
 */
ConnectivityResult_t Connectivity_Update(ConnectivityController_t *controller,
                                         const ConnectivityInputs_t *inputs,
                                         ConnectivityOutputs_t *outputs);

/**
 * @brief Copy current controller state without advancing time or generating events.
 * @param controller Controller to inspect. NULL is accepted as a no-op.
 * @param outputs Destination snapshot. NULL is accepted as a no-op.
 */
void Connectivity_GetOutputs(const ConnectivityController_t *controller,
                             ConnectivityOutputs_t *outputs);

#ifdef __cplusplus
}
#endif

#endif
