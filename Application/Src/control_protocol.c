/**
 * @file control_protocol.c
 * @brief Implementation of the bounded, allocation-free ASCII control parser.
 */
#include "control_protocol.h"

#include <stdint.h>

/**
 * @brief Test whether a protocol byte is accepted as surrounding/token whitespace.
 * @param c Input character.
 * @return true for space, tab, carriage return or line feed.
 */
static bool is_space(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

/**
 * @brief Convert one ASCII lowercase letter to uppercase without locale/libc dependency.
 * @param c Input byte.
 * @return Uppercase equivalent for a-z; all other bytes unchanged.
 */
static char ascii_upper(char c)
{
    if ((c >= 'a') && (c <= 'z')) {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

/**
 * @brief Compare one bounded frame token to an uppercase NUL-terminated literal.
 * @param frame Complete input frame.
 * @param begin Inclusive token start index.
 * @param end Exclusive token end index.
 * @param literal Uppercase literal to compare.
 * @return true only when token length and case-insensitive contents match exactly.
 */
static bool token_equals(const char *frame,
                         size_t begin,
                         size_t end,
                         const char *literal)
{
    size_t i = 0U;
    while ((begin + i) < end) {
        if (literal[i] == '\0') {
            return false;
        }
        if (ascii_upper(frame[begin + i]) != literal[i]) {
            return false;
        }
        ++i;
    }
    return literal[i] == '\0';
}

/**
 * @brief Advance an index past protocol whitespace without crossing the frame boundary.
 * @param frame Input frame.
 * @param index Starting index.
 * @param end Exclusive frame boundary.
 * @return Index of the next non-space byte or @p end.
 */
static size_t skip_spaces(const char *frame, size_t index, size_t end)
{
    while ((index < end) && is_space(frame[index])) {
        ++index;
    }
    return index;
}

/**
 * @brief Find the exclusive end of the next whitespace-delimited token.
 * @param frame Input frame.
 * @param index Token start index.
 * @param end Exclusive frame boundary.
 * @return Exclusive token end index.
 */
static size_t token_end(const char *frame, size_t index, size_t end)
{
    while ((index < end) && !is_space(frame[index])) {
        ++index;
    }
    return index;
}

/** @copydoc ControlProtocol_Parse */
ControlProtocolResult_t ControlProtocol_Parse(const char *frame,
                                              size_t length,
                                              ConnectivityCommand_t *command)
{
    if ((frame == NULL) || (length == 0U)) {
        return CONTROL_PROTOCOL_ERR_ARGUMENT;
    }

    size_t begin = skip_spaces(frame, 0U, length);
    size_t end = length;
    while ((end > begin) && is_space(frame[end - 1U])) {
        --end;
    }
    if (begin == end) {
        return CONTROL_PROTOCOL_ERR_FORMAT;
    }

    const size_t first_end = token_end(frame, begin, end);
    if (token_equals(frame, begin, first_end, "STATUS")) {
        const size_t after = skip_spaces(frame, first_end, end);
        return (after == end) ? CONTROL_PROTOCOL_STATUS_REQUEST : CONTROL_PROTOCOL_ERR_FORMAT;
    }

    if (command == NULL) {
        return CONTROL_PROTOCOL_ERR_ARGUMENT;
    }

    const size_t second_begin = skip_spaces(frame, first_end, end);
    if (second_begin == end) {
        return CONTROL_PROTOCOL_ERR_FORMAT;
    }
    const size_t second_end = token_end(frame, second_begin, end);
    if (skip_spaces(frame, second_end, end) != end) {
        return CONTROL_PROTOCOL_ERR_FORMAT;
    }

    /* Explicit operating-policy aliases satisfy the mode-selection aspect of
     * requirement 1.14 while preserving the same enable/disable state model. */
    if (token_equals(frame, begin, first_end, "MODE")) {
        if (token_equals(frame, second_begin, second_end, "AUTO")) {
            *command = CONNECTIVITY_COMMAND_ENABLE_BOTH;
            return CONTROL_PROTOCOL_OK;
        }
        if (token_equals(frame, second_begin, second_end, "BT")) {
            *command = CONNECTIVITY_COMMAND_BLUETOOTH_ONLY;
            return CONTROL_PROTOCOL_OK;
        }
        if (token_equals(frame, second_begin, second_end, "TC")) {
            *command = CONNECTIVITY_COMMAND_TELECOIL_ONLY;
            return CONTROL_PROTOCOL_OK;
        }
        if (token_equals(frame, second_begin, second_end, "OFF")) {
            *command = CONNECTIVITY_COMMAND_DISABLE_BOTH;
            return CONTROL_PROTOCOL_OK;
        }
        return CONTROL_PROTOCOL_ERR_FORMAT;
    }

    const bool turn_on = token_equals(frame, second_begin, second_end, "ON");
    const bool turn_off = token_equals(frame, second_begin, second_end, "OFF");
    if (!turn_on && !turn_off) {
        return CONTROL_PROTOCOL_ERR_FORMAT;
    }

    if (token_equals(frame, begin, first_end, "BT")) {
        *command = turn_on ? CONNECTIVITY_COMMAND_ENABLE_BLUETOOTH
                           : CONNECTIVITY_COMMAND_DISABLE_BLUETOOTH;
        return CONTROL_PROTOCOL_OK;
    }
    if (token_equals(frame, begin, first_end, "TC")) {
        *command = turn_on ? CONNECTIVITY_COMMAND_ENABLE_TELECOIL
                           : CONNECTIVITY_COMMAND_DISABLE_TELECOIL;
        return CONTROL_PROTOCOL_OK;
    }
    if (token_equals(frame, begin, first_end, "ALL")) {
        *command = turn_on ? CONNECTIVITY_COMMAND_ENABLE_BOTH
                           : CONNECTIVITY_COMMAND_DISABLE_BOTH;
        return CONTROL_PROTOCOL_OK;
    }

    return CONTROL_PROTOCOL_ERR_FORMAT;
}
