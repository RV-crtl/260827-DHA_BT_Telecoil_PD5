/**
 * @file bt_profile.h
 * @brief Hardware-independent BT401 receiver profile sequencing.
 *
 * The module preserves the deterministic AT-command policy used by the earlier
 * Bluetooth prototype while removing UART and delay dependencies. Transmission and
 * timing are injected through callbacks, allowing the same application logic to use
 * a real board adapter later or mock/spy callbacks during PD5 unit testing.
 */
#ifndef BT_PROFILE_H
#define BT_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback used to transmit one complete BT401 command.
 * @param command Command bytes to transmit.
 * @param length Number of bytes in @p command, excluding any C terminator.
 * @param context Caller-owned transport context.
 * @return true when the complete command was accepted by the transport.
 */
typedef bool (*BtProfileSendFn)(const char *command, size_t length, void *context);

/**
 * @brief Optional callback used to wait between profile commands.
 * @param delay_ms Requested delay in milliseconds.
 * @param context Caller-owned timing context.
 */
typedef void (*BtProfileDelayFn)(uint32_t delay_ms, void *context);

/** Result codes returned while applying a Bluetooth module profile. */
typedef enum {
    BT_PROFILE_OK = 0,
    BT_PROFILE_ERR_ARGUMENT,
    BT_PROFILE_ERR_SEND
} BtProfileResult_t;

/** Timing configuration for the deterministic BT401 command sequence. */
typedef struct {
    uint32_t inter_command_delay_ms; /**< Delay inserted between successful commands. */
} BtProfileConfig_t;

/** Number of commands in the preserved BT401 profile. */
#define BT_PROFILE_COMMAND_COUNT 9U

/**
 * @brief Return the default BT401 inter-command timing policy.
 * @return Configuration using a 100 ms delay between consecutive commands.
 */
BtProfileConfig_t BtProfile_DefaultConfig(void);

/**
 * @brief Return one read-only BT401 command by index.
 * @param index Zero-based command index.
 * @return Command string for a valid index, otherwise NULL.
 */
const char *BtProfile_CommandAt(size_t index);

/**
 * @brief Apply the complete BT401 receiver profile through injected callbacks.
 *
 * Commands are sent in the fixed profile order. The delay callback is optional and is
 * invoked only between commands when the configured delay is non-zero. Processing stops
 * immediately if the send callback reports failure.
 *
 * @param config Profile timing configuration.
 * @param send_fn Required transport callback used for each command.
 * @param delay_fn Optional inter-command delay callback.
 * @param context Caller-owned context passed unchanged to both callbacks.
 * @param commands_sent Optional progress output containing the number of successful sends.
 * @return BT_PROFILE_OK on success, or an argument/transport error.
 */
BtProfileResult_t BtProfile_Apply(const BtProfileConfig_t *config,
                                  BtProfileSendFn send_fn,
                                  BtProfileDelayFn delay_fn,
                                  void *context,
                                  size_t *commands_sent);

#ifdef __cplusplus
}
#endif

#endif
