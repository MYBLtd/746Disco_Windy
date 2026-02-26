/**
 * @file  dbg_uart.h
 * @brief Debug UART over USART1 (PA9=TX, AF7) → ST-Link VCP → netcat :20000
 *
 * TX-only; no RX needed.  115200 8N1 matches the ST-Link VCP default.
 * Call dbg_uart_init() once from main() before any other dbg_* call.
 */

#ifndef DBG_UART_H
#define DBG_UART_H

/**
 * @brief  Initialise USART1 PA9 at 115200 8N1.
 */
void dbg_uart_init(void);

/**
 * @brief  Transmit a NUL-terminated string (blocking).
 */
void dbg_puts(const char *s);

/**
 * @brief  Printf-style debug output (max 256 chars per call).
 */
void dbg_printf(const char *fmt, ...);

#endif /* DBG_UART_H */
