/**
 * @file  dbg_uart.c
 * @brief Debug UART – USART1 PA9 TX → ST-Link VCP → netcat localhost:20000
 *
 * APB2 clock = 108 MHz.  BRR auto-calculated by HAL for 115200 baud.
 */

#include "dbg_uart.h"
#include "stm32f7xx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef huart1;

void dbg_uart_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_9;          /* PA9 = USART1_TX */
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);

    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

void dbg_puts(const char *s)
{
    if (!s) return;
    uint16_t len = (uint16_t)strlen(s);
    if (len) HAL_UART_Transmit(&huart1, (const uint8_t *)s, len, 100);
}

void dbg_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    dbg_puts(buf);
}
