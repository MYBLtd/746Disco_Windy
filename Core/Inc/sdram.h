/**
 * @file  sdram.h
 * @brief FMC SDRAM initialisation for IS42S32400F-7BL on STM32F746G-DISCO
 *        (8 MB at 0xC0000000, 32-bit bus, FMC Bank1 SDRAM)
 */

#ifndef SDRAM_H
#define SDRAM_H

#include "stm32f7xx_hal.h"

/**
 * @brief  Initialise FMC GPIO + SDRAM controller and issue the standard
 *         SDRAM mode-register load sequence.
 * @retval HAL_OK on success, HAL_ERROR/HAL_TIMEOUT otherwise.
 */
HAL_StatusTypeDef SDRAM_Init(void);

#endif /* SDRAM_H */
