/**
 * @file board_time.c
 * @brief Polling SysTick implementation for simple board-side delays.
 */
#include "board_time.h"

#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define SYST_CSR REG32(0xE000E010UL)
#define SYST_RVR REG32(0xE000E014UL)
#define SYST_CVR REG32(0xE000E018UL)
#define SYST_CSR_ENABLE    (1UL << 0)
#define SYST_CSR_CLKSOURCE (1UL << 2)
#define SYST_CSR_COUNTFLAG (1UL << 16)

/** @copydoc BoardTime_Init */
void BoardTime_Init(void)
{
    /* HSI reset clock = 16 MHz. Polling-only 1 ms SysTick; no interrupt required. */
    SYST_RVR = 16000UL - 1UL;
    SYST_CVR = 0UL;
    SYST_CSR = SYST_CSR_ENABLE | SYST_CSR_CLKSOURCE;
}

/** @copydoc BoardTime_DelayMs */
void BoardTime_DelayMs(uint32_t delay_ms)
{
    for (uint32_t i = 0U; i < delay_ms; ++i) {
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0UL) {
            /* polling */
        }
    }
}
