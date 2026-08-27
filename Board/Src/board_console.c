/**
 * @file board_console.c
 * @brief Direct-register USART2/ST-LINK VCP and LD2 implementation for NUCLEO-F446RE.
 */
#include "board_console.h"

#include <stddef.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define RCC_BASE            0x40023800UL
#define RCC_AHB1ENR         REG32(RCC_BASE + 0x30UL)
#define RCC_APB1ENR         REG32(RCC_BASE + 0x40UL)

#define GPIOA_BASE          0x40020000UL
#define GPIOA_MODER         REG32(GPIOA_BASE + 0x00UL)
#define GPIOA_OTYPER        REG32(GPIOA_BASE + 0x04UL)
#define GPIOA_OSPEEDR       REG32(GPIOA_BASE + 0x08UL)
#define GPIOA_PUPDR         REG32(GPIOA_BASE + 0x0CUL)
#define GPIOA_BSRR          REG32(GPIOA_BASE + 0x18UL)
#define GPIOA_AFRL          REG32(GPIOA_BASE + 0x20UL)

#define USART2_BASE         0x40004400UL
#define USART2_SR           REG32(USART2_BASE + 0x00UL)
#define USART2_DR           REG32(USART2_BASE + 0x04UL)
#define USART2_BRR          REG32(USART2_BASE + 0x08UL)
#define USART2_CR1          REG32(USART2_BASE + 0x0CUL)

#define RCC_AHB1ENR_GPIOAEN (1UL << 0)
#define RCC_APB1ENR_USART2EN (1UL << 17)
#define USART_SR_TXE        (1UL << 7)
#define USART_CR1_TE        (1UL << 3)
#define USART_CR1_UE        (1UL << 13)

/**
 * @brief Block until USART2 can accept one byte, then write it to the data register.
 * @param character Character to transmit.
 */
static void write_char(char character)
{
    while ((USART2_SR & USART_SR_TXE) == 0UL) {
        /* Wait for transmit data register empty. */
    }
    USART2_DR = (uint32_t)(uint8_t)character;
}

/** @copydoc BoardConsole_Init */
void BoardConsole_Init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 = USART2_TX (AF7). PA5 = Nucleo green LED output. */
    GPIOA_MODER &= (uint32_t)~((uint32_t)((3UL << (2U * 2U)) | (3UL << (5U * 2U))));
    GPIOA_MODER |=  ((2UL << (2U * 2U)) | (1UL << (5U * 2U)));
    GPIOA_OTYPER &= (uint32_t)~((uint32_t)((1UL << 2U) | (1UL << 5U)));
    GPIOA_OSPEEDR |= (3UL << (2U * 2U));
    GPIOA_PUPDR &= (uint32_t)~((uint32_t)(3UL << (2U * 2U)));
    GPIOA_AFRL &= (uint32_t)~((uint32_t)(0xFUL << (2U * 4U)));
    GPIOA_AFRL |=  (7UL << (2U * 4U));

    /* Reset clock is HSI = 16 MHz. BRR=139 gives approximately 115200 baud. */
    USART2_CR1 = 0UL;
    USART2_BRR = 139UL;
    USART2_CR1 = USART_CR1_TE | USART_CR1_UE;

    BoardConsole_SetLed(false);
}

/** @copydoc BoardConsole_WriteString */
void BoardConsole_WriteString(const char *text)
{
    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        write_char(*text++);
    }
}

/** @copydoc BoardConsole_WriteUInt */
void BoardConsole_WriteUInt(uint32_t value)
{
    char buffer[11];
    size_t count = 0U;

    if (value == 0U) {
        write_char('0');
        return;
    }

    while ((value > 0U) && (count < sizeof(buffer))) {
        buffer[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (count > 0U) {
        write_char(buffer[--count]);
    }
}

/** @copydoc BoardConsole_SetLed */
void BoardConsole_SetLed(bool on)
{
    if (on) {
        GPIOA_BSRR = (1UL << 5U);
    } else {
        GPIOA_BSRR = (1UL << (5U + 16U));
    }
}
