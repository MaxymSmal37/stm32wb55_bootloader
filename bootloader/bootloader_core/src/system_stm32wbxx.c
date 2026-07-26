/**
 * @file    system_stm32wbxx.c
 * @brief   Minimal CMSIS system file for STM32WB55.
 *
 * Provides the two symbols the CMSIS startup code and headers expect:
 *   - SystemInit()      : called from Reset_Handler before main()
 *   - SystemCoreClock   : current core clock, in Hz
 *
 * After reset the STM32WB55 runs from the MSI oscillator at 4 MHz. This file
 * keeps that default (no PLL configuration), which is all a simple LED blink
 * needs. Extend SystemInit() / call SystemCoreClockUpdate() when a faster or
 * external clock is required.
 */

#include "stm32wbxx.h"

/* MSI is the default system clock after reset: 4 MHz. */
uint32_t SystemCoreClock = 4000000U;

void SystemInit(void)
{
    /* FPU is not enabled here because the project is built with soft float
       (-mfloat-abi=soft); nothing else needs early init for a blink. */
}

void SystemCoreClockUpdate(void)
{
    /* Clock tree is left at reset defaults, so the core clock is constant. */
    SystemCoreClock = 4000000U;
}
