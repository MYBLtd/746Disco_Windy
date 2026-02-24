/**
 * @file  main.c
 * @brief STM32F746G-DISCO – Windy.com weather display
 *
 * Boot sequence
 *   1. HAL_Init()            – SysTick @ 1 ms
 *   2. SystemClock_Config()  – 216 MHz SYSCLK + PLLSAI 9.6 MHz LTDC pixel clock
 *   3. windy_display_show()  – LTDC pointed at embedded RGB565 weather image
 *   4. idle loop             – image displayed indefinitely
 *
 * To refresh the image:
 *   cd tools && python3 windy_render.py   # fetches live data, regenerates windy_img.h
 *   make && st-flash write build/display_test.bin 0x08000000
 */

#include "main.h"
#include "windy_display.h"

static void SystemClock_Config(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    if (windy_display_show() != 0)
        Error_Handler();

    /* Image is now live on the LCD.  The LTDC refreshes it autonomously
       from Flash; the CPU has nothing further to do. */
    while (1)
    {
        __WFI();   /* wait-for-interrupt: sleep until SysTick wakes us */
    }
}

/**
 * @brief  Configure system clocks.
 *
 * Source  : HSE 25 MHz
 * SYSCLK  : 216 MHz  (PLL M=25 N=432 P=2)
 * AHB     : 216 MHz
 * APB1    : 54 MHz  (/4)
 * APB2    : 108 MHz (/2)
 *
 * LTDC clock via PLLSAI:
 *   PLLSAI N=192, R=5 → PLLSAI_VCO = 192 MHz
 *   PLLSAIDIVR = /4   → LTDC_CLK   = 9.6 MHz  (panel spec: 9 MHz ± 10%)
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInit = {0};
    RCC_ClkInitTypeDef RCC_ClkInit = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInit.HSEState       = RCC_HSE_ON;
    RCC_OscInit.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInit.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInit.PLL.PLLM       = 25;
    RCC_OscInit.PLL.PLLN       = 432;
    RCC_OscInit.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInit.PLL.PLLQ       = 9;
    if (HAL_RCC_OscConfig(&RCC_OscInit) != HAL_OK)
        Error_Handler();

    /* Activate Over-Drive for 216 MHz */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
        Error_Handler();

    RCC_ClkInit.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInit.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInit.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInit.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInit.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInit, FLASH_LATENCY_7) != HAL_OK)
        Error_Handler();

    /* PLLSAI for LTDC */
    PeriphClkInit.PeriphClockSelection  = RCC_PERIPHCLK_LTDC;
    PeriphClkInit.PLLSAI.PLLSAIN        = 192;
    PeriphClkInit.PLLSAI.PLLSAIR        = 5;
    PeriphClkInit.PLLSAIDivR            = RCC_PLLSAIDIVR_4;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        Error_Handler();
}

/* ── HAL callbacks ───────────────────────────────────────────────── */

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif
