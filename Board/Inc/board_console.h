/**
 * @file board_console.h
 * @brief Minimal NUCLEO-F446RE serial-output and success-LED adapter.
 *
 * Test/application text is emitted over USART2 TX on PA2, which is connected to the ST-LINK
 * virtual COM port. LD2 on PA5 provides a simple visual pass indicator after a completed run.
 */
#ifndef BOARD_CONSOLE_H
#define BOARD_CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Configure USART2 TX at 115200 8-N-1 and initialise LD2 off. */
void BoardConsole_Init(void);

/**
 * @brief Write a NUL-terminated string synchronously to the ST-LINK virtual COM port.
 * @param text String to write; NULL is accepted as a no-op.
 */
void BoardConsole_WriteString(const char *text);

/**
 * @brief Write an unsigned decimal integer without using printf().
 * @param value Value to render in base 10.
 */
void BoardConsole_WriteUInt(uint32_t value);

/**
 * @brief Set the NUCLEO LD2 pass/fail indicator.
 * @param on True to turn LD2 on; false to turn it off.
 */
void BoardConsole_SetLed(bool on);

#ifdef __cplusplus
}
#endif

#endif
