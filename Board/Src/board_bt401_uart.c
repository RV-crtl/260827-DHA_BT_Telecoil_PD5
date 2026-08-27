/**
 * @file board_bt401_uart.c
 * @brief Direct-register USART1 implementation used only by the thin F446 board adapter.
 */
#include "board_bt401_uart.h"
#include "board_time.h"

#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define RCC_BASE             0x40023800UL
#define RCC_AHB1ENR          REG32(RCC_BASE + 0x30UL)
#define RCC_APB2ENR          REG32(RCC_BASE + 0x44UL)

#define GPIOA_BASE           0x40020000UL
#define GPIOA_MODER          REG32(GPIOA_BASE + 0x00UL)
#define GPIOA_OTYPER         REG32(GPIOA_BASE + 0x04UL)
#define GPIOA_OSPEEDR        REG32(GPIOA_BASE + 0x08UL)
#define GPIOA_PUPDR          REG32(GPIOA_BASE + 0x0CUL)
#define GPIOA_AFRH           REG32(GPIOA_BASE + 0x24UL)

#define USART1_BASE          0x40011000UL
#define USART1_SR            REG32(USART1_BASE + 0x00UL)
#define USART1_DR            REG32(USART1_BASE + 0x04UL)
#define USART1_BRR           REG32(USART1_BASE + 0x08UL)
#define USART1_CR1           REG32(USART1_BASE + 0x0CUL)

#define RCC_AHB1ENR_GPIOAEN  (1UL << 0)
#define RCC_APB2ENR_USART1EN (1UL << 4)
#define USART_SR_TXE         (1UL << 7)
#define USART_SR_TC          (1UL << 6)
#define USART_CR1_TE         (1UL << 3)
#define USART_CR1_RE         (1UL << 2)
#define USART_CR1_UE         (1UL << 13)

/** @copydoc BoardBt401Uart_Init */
void BoardBt401Uart_Init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9/PA10 = USART1 TX/RX, AF7. */
    GPIOA_MODER &= (uint32_t)~((uint32_t)((3UL << (9U * 2U)) | (3UL << (10U * 2U))));
    GPIOA_MODER |=  ((2UL << (9U * 2U)) | (2UL << (10U * 2U)));
    GPIOA_OTYPER &= (uint32_t)~((uint32_t)((1UL << 9U) | (1UL << 10U)));
    GPIOA_OSPEEDR |= (3UL << (9U * 2U)) | (3UL << (10U * 2U));
    GPIOA_PUPDR &= (uint32_t)~((uint32_t)((3UL << (9U * 2U)) | (3UL << (10U * 2U))));
    GPIOA_AFRH &= (uint32_t)~((uint32_t)((0xFUL << ((9U - 8U) * 4U)) | (0xFUL << ((10U - 8U) * 4U))));
    GPIOA_AFRH |=  (7UL << ((9U - 8U) * 4U)) | (7UL << ((10U - 8U) * 4U));

    USART1_CR1 = 0UL;
    USART1_BRR = 139UL;
    USART1_CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/** @copydoc BoardBt401Uart_Send */
bool BoardBt401Uart_Send(const char *command, size_t length, void *context)
{
    (void)context;
    if ((command == NULL) && (length > 0U)) {
        return false;
    }

    for (size_t i = 0U; i < length; ++i) {
        while ((USART1_SR & USART_SR_TXE) == 0UL) {
            /* wait */
        }
        USART1_DR = (uint32_t)(uint8_t)command[i];
    }
    while ((USART1_SR & USART_SR_TC) == 0UL) {
        /* wait for complete frame */
    }
    return true;
}

/** @copydoc BoardBt401Uart_Delay */
void BoardBt401Uart_Delay(uint32_t delay_ms, void *context)
{
    (void)context;
    BoardTime_DelayMs(delay_ms);
}
