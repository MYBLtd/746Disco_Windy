/**
 * @file  stm32f7xx_hal_conf.h
 * @brief HAL module enable / configuration for the display test project.
 *        Only the peripherals actually used are compiled in.
 */

#ifndef STM32F7XX_HAL_CONF_H
#define STM32F7XX_HAL_CONF_H

/* ── Module enable ───────────────────────────────────────────────── */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED
#define HAL_DMA2D_MODULE_ENABLED
#define HAL_SDRAM_MODULE_ENABLED

/* ── HSE / HSI / LSE values ──────────────────────────────────────── */
#if !defined(HSE_VALUE)
  #define HSE_VALUE       25000000U  /* STM32F746G-DISCO: 25 MHz crystal */
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT  100U  /* ms */
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE       16000000U
#endif
#if !defined(LSI_VALUE)
  #define LSI_VALUE           32000U
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE           32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT 5000U
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE  12288000U
#endif

/* ── SysTick / tick frequency ────────────────────────────────────── */
#define TICK_INT_PRIORITY       0x0FU   /* lowest priority */
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U
#define ART_ACCELERATOR_ENABLE  1U

/* ── Ethernet – not used ─────────────────────────────────────────── */
#define MAC_ADDR0    2U
#define MAC_ADDR1    0U
#define MAC_ADDR2    0U
#define MAC_ADDR3    0U
#define MAC_ADDR4    0U
#define MAC_ADDR5    0U

#define ETH_RX_BUFFER_SIZE  1524U

/* ── Include HAL peripheral headers ─────────────────────────────── */
#include "stm32f7xx_hal_rcc.h"
#include "stm32f7xx_hal_rcc_ex.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_cortex.h"
#include "stm32f7xx_hal_flash.h"
#include "stm32f7xx_hal_flash_ex.h"
#include "stm32f7xx_hal_pwr.h"
#include "stm32f7xx_hal_pwr_ex.h"
#include "stm32f7xx_hal_ltdc.h"
#include "stm32f7xx_hal_dma2d.h"
#include "stm32f7xx_hal_sdram.h"

/* assert_param is a no-op for this test */
#define assert_param(expr)  ((void)0U)

#endif /* STM32F7XX_HAL_CONF_H */
