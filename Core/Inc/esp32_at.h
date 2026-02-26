/**
 * @file  esp32_at.h
 * @brief Thin ESP32 AT-command driver over USART6.
 *
 * Hardware wiring (3.3 V logic):
 *   STM32 PC6 (USART6_TX, AF8) → ESP32 RX
 *   STM32 PC7 (USART6_RX, AF8) → ESP32 TX
 *   Common GND
 *
 * Requires HAL_UART_MODULE_ENABLED and HAL_UART_MODULE_ENABLED
 * in stm32f7xx_hal_conf.h.
 */

#ifndef ESP32_AT_H
#define ESP32_AT_H

#include <stdint.h>

/**
 * @brief  Initialise USART6 GPIO (PC6/PC7) and UART peripheral at 115200 8N1.
 * @retval 0 on success, -1 on HAL error.
 */
int esp32_init(void);

/**
 * @brief  Connect to a WiFi access point.
 *         Sends AT+RST, AT+CWMODE=1, AT+CWJAP.
 * @param  ssid  NUL-terminated network name.
 * @param  pass  NUL-terminated WPA2 passphrase.
 * @retval 0 on success (got IP), -1 on timeout/error.
 */
int esp32_connect_wifi(const char *ssid, const char *pass);

/**
 * @brief  Open a TCP connection and perform an HTTP/1.1 GET request.
 *         Stores the HTTP response body (JSON) in @p resp_buf.
 * @param  host      NUL-terminated hostname (ASCII, no https://).
 * @param  path      NUL-terminated request path (starts with /).
 * @param  resp_buf  Output buffer for the response body.
 * @param  buf_len   Size of resp_buf in bytes.
 * @retval 0 on success, -1 on error.
 */
int esp32_https_get(const char *host, const char *path,
                    char *resp_buf, uint16_t buf_len);

/**
 * @brief  Download a raw binary file via HTTP GET and write it directly to
 *         an SDRAM destination address.
 *
 *         Uses AT transparent passthrough (AT+CIPMODE=1) so the AT firmware
 *         forwards all TCP payload bytes over UART without +IPD framing.
 *         The HTTP response header is detected byte-by-byte and discarded;
 *         only the body bytes are written to @p dst_addr.
 *         Uses HTTP/1.0 so the server closes after the response (no chunked
 *         encoding, no Content-Length parsing required).
 *
 * @param  host           NUL-terminated server IP or hostname.
 * @param  port           TCP port (e.g. 8080).
 * @param  path           NUL-terminated request path (e.g. "/windy.bin").
 * @param  dst_addr       SDRAM destination address (e.g. LCD_BACK_BUFFER).
 * @param  expected_bytes Number of body bytes expected (480×272×2 = 261,120).
 * @retval 0 on success, -1 on timeout/error.
 */
int esp32_http_get_image(const char *host, uint16_t port, const char *path,
                         uint32_t dst_addr, uint32_t expected_bytes);

#endif /* ESP32_AT_H */
