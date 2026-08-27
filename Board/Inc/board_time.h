/**
 * @file board_time.h
 * @brief Polling-only SysTick timing adapter used by the optional board demonstration.
 */
#ifndef BOARD_TIME_H
#define BOARD_TIME_H

#include <stdint.h>

/** @brief Configure SysTick for a 1 ms polling period from the 16 MHz HSI reset clock. */
void BoardTime_Init(void);

/**
 * @brief Busy-wait for an integer number of milliseconds.
 * @param delay_ms Number of 1 ms SysTick periods to wait.
 */
void BoardTime_DelayMs(uint32_t delay_ms);

#endif
