/**
 * @file  stm32f7xx_it.c
 * @brief Interrupt handlers.  Only SysTick is needed for HAL_Delay().
 *        All other handlers are handled by the weak Default_Handler in startup.s
 */

#include "main.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
}
