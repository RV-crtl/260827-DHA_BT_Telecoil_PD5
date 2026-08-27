/**
 * @file connectivity_service.h
 * @brief High-level hardware-independent Bluetooth/telecoil application facade.
 *
 * The service composes the finite-state controller, telecoil detector/filter, signal-quality
 * classifier, source control and audio routing into one integration API. External hardware is
 * represented only by caller-supplied link observations and PCM buffers; the module owns no
 * UART, GPIO, DMA, I2S, RTOS or STM32 HAL dependency.
 *
 * The caller supplies a monotonic millisecond timestamp and should service the application at
 * least every DESIGN_SERVICE_INTERVAL_MAX_MS. Runtime diagnostics record the maximum observed
 * interval and any missed service deadlines. Telecoil loss is time-qualified so fault timing is
 * independent of PCM block length.
 */
#ifndef CONNECTIVITY_SERVICE_H
#define CONNECTIVITY_SERVICE_H

#include "connectivity_controller.h"
#include "signal_quality.h"
#include "telecoil_detector.h"
#include "telecoil_filter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes returned by top-level service operations. */
typedef enum {
    CONNECTIVITY_SERVICE_OK = 0,
    CONNECTIVITY_SERVICE_ERR_ARGUMENT,
    CONNECTIVITY_SERVICE_ERR_SAMPLE_RATE,
    CONNECTIVITY_SERVICE_ERR_STATE,
    CONNECTIVITY_SERVICE_STALE_BT_FRAME
} ConnectivityServiceResult_t;

/** User/system control commands accepted by ConnectivityService_ApplyCommand(). */
typedef enum {
    CONNECTIVITY_COMMAND_ENABLE_BLUETOOTH = 0,
    CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH,
    CONNECTIVITY_COMMAND_ENABLE_TELECOIL,
    CONNECTIVITY_COMMAND_DISABLE_TELECOIL,
    CONNECTIVITY_COMMAND_ENABLE_BOTH,
    CONNECTIVITY_COMMAND_DISABLE_BOTH,
    CONNECTIVITY_COMMAND_BLUETOOTH_ONLY,
    CONNECTIVITY_COMMAND_TELECOIL_ONLY
} ConnectivityCommand_t;

/** Hardware-independent Bluetooth/interface observations supplied by the integration layer. */
typedef struct {
    uint32_t now_ms;              /**< Current millisecond timestamp. */
    bool interfaces_ready;        /**< Bluetooth and telecoil interfaces completed startup. */
    bool paired_device_available; /**< A previously paired Bluetooth device is available. */
    bool bluetooth_connected;     /**< Bluetooth audio link is currently valid. */
} ConnectivityLinkState_t;

/** Runtime counters used to make integration behaviour observable and testable. */
typedef struct {
    uint32_t controller_updates;
    uint32_t source_switches;
    uint32_t faults_entered;
    uint32_t recoveries;
    uint32_t bt_connect_requests;
    uint32_t bt_reconnect_requests;
    uint32_t bt_frames_processed;
    uint32_t stale_bt_frames_dropped;
    uint32_t telecoil_blocks_processed;
    uint32_t telecoil_valid_transitions;
    uint32_t service_deadline_misses;
    uint32_t max_service_interval_ms;
} ConnectivityDiagnostics_t;

/** Persistent state owned by one complete connectivity-service instance. */
typedef struct {
    ConnectivityController_t controller;
    TelecoilDetector_t telecoil_detector;
    TelecoilFilter_t telecoil_filter;
    ConnectivityLinkState_t link_state;
    ConnectivityOutputs_t last_outputs;
    ConnectivityDiagnostics_t diagnostics;
    TelecoilBlockMetrics_t last_telecoil_metrics;
    SignalQualityClass_t telecoil_quality;
    uint32_t telecoil_noise_rms;
    uint32_t sample_rate_hz;
    int32_t bluetooth_gain_q15;
    int32_t telecoil_gain_q15;
    uint32_t last_service_ms;
    uint32_t telecoil_absent_since_ms;
    bool telecoil_valid_timed;
    bool telecoil_absence_tracking;
    bool initialised;
} ConnectivityService_t;

/**
 * @brief Initialise the complete portable Bluetooth/telecoil application service.
 *
 * The controller, telecoil detector and telecoil filter are initialised together. Both
 * source gains start at Q15 unity and the telecoil signal-quality result starts UNKNOWN
 * until a non-zero noise-floor estimate is supplied.
 *
 * @param service Service instance to initialise.
 * @param sample_rate_hz Required audio rate; only 16 kHz and 48 kHz are accepted.
 * @param now_ms Initial caller-supplied millisecond timestamp.
 * @return CONNECTIVITY_SERVICE_OK on success, or an argument/sample-rate/state error.
 */
ConnectivityServiceResult_t ConnectivityService_Init(ConnectivityService_t *service,
                                                      uint32_t sample_rate_hz,
                                                      uint32_t now_ms);

/**
 * @brief Update link/interface observations and advance the state controller once.
 *
 * The function records the service interval for deadline diagnostics, rejects backwards
 * timestamps, updates stored link state, then performs one deterministic controller step.
 *
 * @param service Initialised service instance.
 * @param link_state Current interface/Bluetooth observations and timestamp.
 * @return CONNECTIVITY_SERVICE_OK on success, otherwise an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_Step(ConnectivityService_t *service,
                                                      const ConnectivityLinkState_t *link_state);

/**
 * @brief Apply one source-enable/mode command and immediately re-evaluate application state.
 *
 * Intentional source changes are passed to Connectivity_SetEnabled(), so disabling an active
 * source is not misclassified as a hardware fault. Bluetooth-only and telecoil-only commands
 * are applied atomically.
 *
 * @param service Initialised service instance.
 * @param command Command to apply.
 * @param now_ms Current millisecond timestamp.
 * @return CONNECTIVITY_SERVICE_OK, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_ApplyCommand(ConnectivityService_t *service,
                                                              ConnectivityCommand_t command,
                                                              uint32_t now_ms);

/**
 * @brief Configure independent non-negative Q15 gains for Bluetooth and telecoil audio.
 * @param service Initialised service instance.
 * @param bluetooth_gain_q15 Bluetooth gain; 32768 represents unity.
 * @param telecoil_gain_q15 Telecoil gain; 32768 represents unity.
 * @return CONNECTIVITY_SERVICE_OK, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_SetGains(ConnectivityService_t *service,
                                                          int32_t bluetooth_gain_q15,
                                                          int32_t telecoil_gain_q15);

/**
 * @brief Store the measured/reference telecoil noise RMS used by the 20 dB classifier.
 *
 * A value of zero leaves the service quality classification UNKNOWN; the function does not
 * claim to measure physical SNR itself.
 *
 * @param service Initialised service instance.
 * @param noise_rms Reference noise-floor RMS amplitude in the same units as telecoil PCM metrics.
 * @return CONNECTIVITY_SERVICE_OK, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_SetTelecoilNoiseFloor(ConnectivityService_t *service,
                                                                       uint32_t noise_rms);

/**
 * @brief Analyse, qualify, filter and condition one telecoil PCM block.
 *
 * The raw detector retains activation hysteresis. Once the time-qualified service state is
 * valid, a clearly absent signal must persist for DESIGN_TELECOIL_LOSS_CONFIRM_MS before
 * invalidation. The controller is stepped using that qualified validity, and audio is routed
 * only when M2 has Telecoil selected and mute is clear. Otherwise the output block is zeroed.
 *
 * @param service Initialised service instance.
 * @param input Input mono S16 telecoil samples.
 * @param output Output mono S16 buffer with @p sample_count entries.
 * @param sample_count Number of samples; must be greater than zero.
 * @param now_ms Current millisecond timestamp.
 * @return CONNECTIVITY_SERVICE_OK on success, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_ProcessTelecoilBlock(ConnectivityService_t *service,
                                                                      const int16_t *input,
                                                                      int16_t *output,
                                                                      size_t sample_count,
                                                                      uint32_t now_ms);

/**
 * @brief Route and gain one Bluetooth PCM block when Bluetooth is the active source.
 *
 * The application rejects a block when `now_ms - received_ms` exceeds the 100 ms Bluetooth
 * latency guard. Rejected or otherwise unroutable blocks are zeroed. This is a software stale-
 * frame guard, not a substitute for later physical RF-to-I2S latency measurement.
 *
 * @param service Initialised service instance.
 * @param input Input mono S16 Bluetooth PCM samples.
 * @param output Output mono S16 buffer with @p sample_count entries.
 * @param sample_count Number of samples; must be greater than zero.
 * @param received_ms Timestamp at which the PCM block entered the application boundary.
 * @param now_ms Current processing timestamp.
 * @return OK, STALE_BT_FRAME, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_ProcessBluetoothBlock(ConnectivityService_t *service,
                                                                       const int16_t *input,
                                                                       int16_t *output,
                                                                       size_t sample_count,
                                                                       uint32_t received_ms,
                                                                       uint32_t now_ms);

/**
 * @brief Copy the most recent controller output snapshot without changing state.
 * @param service Initialised service instance.
 * @param outputs Destination for mode/source/fault/mute/request status.
 * @return CONNECTIVITY_SERVICE_OK, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_GetStatus(const ConnectivityService_t *service,
                                                           ConnectivityOutputs_t *outputs);

/**
 * @brief Clear runtime diagnostics and rebase service-interval measurement to current time.
 *
 * Operational state, source selection, filters and detector history are intentionally preserved.
 *
 * @param service Initialised service instance.
 * @return CONNECTIVITY_SERVICE_OK, or an argument/state error.
 */
ConnectivityServiceResult_t ConnectivityService_ResetDiagnostics(ConnectivityService_t *service);

#ifdef __cplusplus
}
#endif

#endif
