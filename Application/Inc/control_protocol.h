/**
 * @file control_protocol.h
 * @brief Bounded ASCII control decoder for a future UART, GPIO or UI adapter.
 *
 * Parsing is case-insensitive, accepts surrounding whitespace/CR/LF, requires no NUL
 * terminator, performs no allocation and does not depend on libc string helpers. The
 * protocol translates external text into portable ConnectivityCommand_t values; it does
 * not own any UART peripheral or input task.
 */
#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include "connectivity_service.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes returned while decoding the control protocol. */
typedef enum {
    CONTROL_PROTOCOL_OK = 0,
    CONTROL_PROTOCOL_STATUS_REQUEST,
    CONTROL_PROTOCOL_ERR_ARGUMENT,
    CONTROL_PROTOCOL_ERR_FORMAT
} ControlProtocolResult_t;

/**
 * @brief Parse one bounded ASCII control frame.
 *
 * Accepted commands are `BT ON`, `BT OFF`, `TC ON`, `TC OFF`, `ALL ON`, `ALL OFF`,
 * `MODE AUTO`, `MODE BT`, `MODE TC`, `MODE OFF`, and `STATUS`. `STATUS` returns
 * CONTROL_PROTOCOL_STATUS_REQUEST without modifying @p command. All other valid frames
 * require a non-NULL output pointer and produce one ConnectivityCommand_t value.
 *
 * @param frame Input byte buffer; it does not need to be NUL terminated.
 * @param length Number of valid bytes in @p frame.
 * @param command Destination for non-STATUS application commands.
 * @return CONTROL_PROTOCOL_OK, STATUS_REQUEST, or an argument/format error.
 */
ControlProtocolResult_t ControlProtocol_Parse(const char *frame,
                                              size_t length,
                                              ConnectivityCommand_t *command);

#ifdef __cplusplus
}
#endif

#endif
