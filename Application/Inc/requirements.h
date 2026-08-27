/**
 * @file requirements.h
 * @brief Requirement-derived constants and compile-time timing contracts.
 *
 * This header is the single source of truth for timing, sample-rate, quality and power
 * constants used by the portable application. Values mirror the subsystem requirements
 * while `DESIGN_*` values document implementation choices made to satisfy those limits.
 *
 * The PD5 application is scheduler-independent, so the integration contract is expressed
 * as a maximum caller service interval. A valid telecoil may be declared absent only after
 * a 40 ms confirmation interval; with a maximum 40 ms observation interval the conservative
 * software detection bound is 80 ms, below the 100 ms fault-entry requirement. Static
 * assertions prevent later edits from silently violating these relationships.
 */
#ifndef REQUIREMENTS_H
#define REQUIREMENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @name Prototype Design 2 requirement constants */
/** @{ */
#define REQ_STARTUP_TIMEOUT_MS            UINT32_C(3000)  /**< Req. 1.1 */
#define REQ_BT_CONNECT_TIMEOUT_MS         UINT32_C(5000)  /**< Req. 1.2 */
#define REQ_SOURCE_SWITCH_MAX_MS          UINT32_C(100)   /**< Req. 1.8 / fault-detection budget. */
#define REQ_BT_AUDIO_LATENCY_MAX_MS       UINT32_C(100)   /**< Req. 1.9 */
#define REQ_BT_RECONNECT_PERIOD_MS        UINT32_C(2000)  /**< Req. 1.18 */
#define REQ_FAULT_RECOVERY_MAX_MS         UINT32_C(2000)  /**< Req. 1.19 */
#define REQ_FAULT_TO_IDLE_TIMEOUT_MS      UINT32_C(5000)  /**< Req. 1.20 */
#define REQ_FAULT_MUTE_MAX_MS             UINT32_C(50)    /**< Req. 1.17 */
#define REQ_TELECOIL_MIN_SNR_DB           UINT32_C(20)    /**< Req. 1.10 */
#define REQ_IDLE_POWER_MAX_MW              UINT32_C(10)    /**< Req. 1.12; verified physically later. */
#define REQ_SAMPLE_RATE_16K_HZ             UINT32_C(16000) /**< Req. 1.11 */
#define REQ_SAMPLE_RATE_48K_HZ             UINT32_C(48000) /**< Req. 1.11 */
/** @} */

/** @name Portable implementation timing contract */
/** @{ */
#define DESIGN_SERVICE_INTERVAL_MAX_MS       UINT32_C(40) /**< Maximum intended service-call interval. */
#define DESIGN_TELECOIL_LOSS_CONFIRM_MS      UINT32_C(40) /**< Continuous absence required before invalidation. */
#define DESIGN_TELECOIL_FAULT_WORST_CASE_MS  (DESIGN_SERVICE_INTERVAL_MAX_MS + \
                                               DESIGN_TELECOIL_LOSS_CONFIRM_MS)
#define DESIGN_RECOVERY_STABLE_MS             UINT32_C(100) /**< Stable-valid debounce before M4 -> M2. */
/** @} */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(DESIGN_RECOVERY_STABLE_MS <= REQ_FAULT_RECOVERY_MAX_MS,
               "Recovery debounce must remain inside requirement 1.19");
_Static_assert(DESIGN_TELECOIL_FAULT_WORST_CASE_MS <= REQ_SOURCE_SWITCH_MAX_MS,
               "Telecoil loss detection must remain inside requirement 1.16");
_Static_assert(DESIGN_SERVICE_INTERVAL_MAX_MS <= REQ_FAULT_MUTE_MAX_MS,
               "Service cadence must not exceed the fault-mute budget");
#endif

#ifdef __cplusplus
}
#endif

#endif
