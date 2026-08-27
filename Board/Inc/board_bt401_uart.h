/**
 * @file board_bt401_uart.h
 * @brief Minimal STM32F446RE USART1 adapter for optional BT401 profile transmission.
 *
 * This board-specific layer is deliberately outside `Application/`. The assessed application
 * remains portable and sees the Bluetooth profile transport only through injected callbacks.
 */
#ifndef BOARD_BT401_UART_H
#define BOARD_BT401_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Configure PA9/PA10 as USART1 at 115200 baud using the 16 MHz HSI reset clock.
 */
void BoardBt401Uart_Init(void);

/**
 * @brief Transmit one command synchronously over USART1.
 * @param command Command buffer; may be NULL only when @p length is zero.
 * @param length Number of bytes to transmit.
 * @param context Unused callback context, retained for BtProfileSendFn compatibility.
 * @return true when the complete command has been transmitted.
 */
bool BoardBt401Uart_Send(const char *command, size_t length, void *context);

/**
 * @brief Delay between BT401 profile commands using the board timing adapter.
 * @param delay_ms Delay duration in milliseconds.
 * @param context Unused callback context, retained for BtProfileDelayFn compatibility.
 */
void BoardBt401Uart_Delay(uint32_t delay_ms, void *context);

#endif
