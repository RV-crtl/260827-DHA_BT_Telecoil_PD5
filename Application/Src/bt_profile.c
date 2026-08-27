/**
 * @file bt_profile.c
 * @brief Implementation of the deterministic, callback-driven BT401 profile sequence.
 */
#include "bt_profile.h"

static const char *const k_bt401_commands[BT_PROFILE_COMMAND_COUNT] = {
    "AT+CM01\r\n",
    "AT+B200\r\n",
    "AT+B301\r\n",
    "AT+B501\r\n",
    "AT+B401\r\n",
    "AT+CN00\r\n",
    "AT+CA30\r\n",
    "AT+CU00\r\n",
    "AT+CS01\r\n"
};

/**
 * @brief Measure one static NUL-terminated command without a libc string dependency.
 * @param command Valid static command string.
 * @return Number of bytes before the terminator.
 */
static size_t command_length(const char *command)
{
    size_t length = 0U;
    while (command[length] != '\0') {
        ++length;
    }
    return length;
}

/** @copydoc BtProfile_DefaultConfig */
BtProfileConfig_t BtProfile_DefaultConfig(void)
{
    BtProfileConfig_t config;
    config.inter_command_delay_ms = 100U;
    return config;
}

/** @copydoc BtProfile_CommandAt */
const char *BtProfile_CommandAt(size_t index)
{
    if (index >= BT_PROFILE_COMMAND_COUNT) {
        return NULL;
    }
    return k_bt401_commands[index];
}

/** @copydoc BtProfile_Apply */
BtProfileResult_t BtProfile_Apply(const BtProfileConfig_t *config,
                                  BtProfileSendFn send_fn,
                                  BtProfileDelayFn delay_fn,
                                  void *context,
                                  size_t *commands_sent)
{
    if (commands_sent != NULL) {
        *commands_sent = 0U;
    }
    if ((config == NULL) || (send_fn == NULL)) {
        return BT_PROFILE_ERR_ARGUMENT;
    }

    for (size_t i = 0U; i < BT_PROFILE_COMMAND_COUNT; ++i) {
        const char *const command = k_bt401_commands[i];
        if (!send_fn(command, command_length(command), context)) {
            return BT_PROFILE_ERR_SEND;
        }

        if (commands_sent != NULL) {
            *commands_sent = i + 1U;
        }

        if ((delay_fn != NULL) && ((i + 1U) < BT_PROFILE_COMMAND_COUNT) &&
            (config->inter_command_delay_ms > 0U)) {
            delay_fn(config->inter_command_delay_ms, context);
        }
    }

    return BT_PROFILE_OK;
}
