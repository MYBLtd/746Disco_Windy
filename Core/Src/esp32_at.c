/**
 * @file  esp32_at.c
 * @brief ESP32 AT-command driver for WiFi + HTTP GET.
 *
 * UART: USART6, APB2 bus (108 MHz), PC6=TX AF8, PC7=RX AF8, 115 200 8N1.
 *
 * All receive operations use a byte-by-byte polling loop with an idle
 * timeout: we read until no byte arrives for IDLE_MS milliseconds.
 * This is simple and reliable for a bare-metal, no-RTOS application.
 *
 * RX buffer is static in SRAM1 (not DTCM) – DMA-safe if needed later.
 *
 * Debug output goes to USART1 (PA9, ST-Link VCP → netcat :20000).
 */

#include "esp32_at.h"
#include "dbg_uart.h"
#include "stm32f7xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ── Internal state ──────────────────────────────────────────────── */

static UART_HandleTypeDef huart6;

/* RX scratch buffer – static in SRAM1 (not DTCM, which DMA can't access) */
static char s_rx[2048];

/* ── Low-level helpers ───────────────────────────────────────────── */

static int esp_recv(char *buf, uint16_t max_len, uint32_t idle_ms)
{
    uint16_t n = 0;
    uint32_t t_last = HAL_GetTick();

    while (n < max_len - 1u) {
        uint8_t b;
        if (HAL_UART_Receive(&huart6, &b, 1, 1) == HAL_OK) {
            buf[n++] = (char)b;
            t_last = HAL_GetTick();
        } else if (HAL_GetTick() - t_last >= idle_ms) {
            break;
        }
    }
    buf[n] = '\0';
    return n;
}

static int esp_send(const char *s)
{
    uint16_t len = (uint16_t)strlen(s);
    return (HAL_UART_Transmit(&huart6, (const uint8_t *)s, len, 5000) == HAL_OK)
           ? 0 : -1;
}

/* Escape characters that AT+CWJAP treats specially: " \ , */
static void at_escape(const char *src, char *dst, size_t dst_len)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_len - 2u; i++) {
        if (src[i] == '"' || src[i] == '\\' || src[i] == ',')
            dst[j++] = '\\';
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

static int at_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    /* flush stale input */
    uint8_t dummy;
    while (HAL_UART_Receive(&huart6, &dummy, 1, 1) == HAL_OK) {}

    if (cmd) {
        dbg_printf("[AT>] %s\r\n", cmd);
        if (esp_send(cmd) != 0) return -1;
        if (esp_send("\r\n") != 0) return -1;
    }

    uint32_t deadline = HAL_GetTick() + timeout_ms;
    int      total    = 0;
    s_rx[0] = '\0';

    while (HAL_GetTick() < deadline) {
        int n = esp_recv(s_rx + total, (uint16_t)(sizeof(s_rx) - (uint16_t)total), 200);
        total += n;
        if (n > 0 && expect && strstr(s_rx, expect)) {
            dbg_printf("[AT<] OK (%s found)\r\n", expect);
            return 0;
        }
        if (n == 0) break;
    }

    if (expect && strstr(s_rx, expect)) {
        dbg_printf("[AT<] OK (%s found)\r\n", expect);
        return 0;
    }

    /* Log first 80 chars of the unexpected response */
    char preview[81];
    strncpy(preview, s_rx, 80);
    preview[80] = '\0';
    /* Replace control chars with '.' for clean output */
    for (int i = 0; preview[i]; i++)
        if ((uint8_t)preview[i] < 0x20 && preview[i] != '\r' && preview[i] != '\n')
            preview[i] = '.';
    dbg_printf("[AT!] TIMEOUT waiting for '%s' | got: %s\r\n",
               expect ? expect : "(none)", preview);
    return -1;
}

/* ── Public API ──────────────────────────────────────────────────── */

int esp32_init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &g);

    huart6.Instance          = USART6;
    huart6.Init.BaudRate     = 115200;
    huart6.Init.WordLength   = UART_WORDLENGTH_8B;
    huart6.Init.StopBits     = UART_STOPBITS_1;
    huart6.Init.Parity       = UART_PARITY_NONE;
    huart6.Init.Mode         = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        dbg_puts("[ESP] USART6 init failed!\r\n");
        return -1;
    }

    dbg_puts("[ESP] USART6 ready (PC6=TX PC7=RX 115200)\r\n");
    return 0;
}

int esp32_connect_wifi(const char *ssid, const char *pass)
{
    /* Send a bare CRLF first to flush any partial command in the ESP32's
       UART buffer (can cause the first real command to return ERROR). */
    esp_send("\r\n");
    HAL_Delay(100);
    { uint8_t dummy; while (HAL_UART_Receive(&huart6, &dummy, 1, 1) == HAL_OK) {} }

    /* Retry AT up to 3 times – first attempt can fail on UART startup */
    dbg_puts("[ESP] Checking AT...\r\n");
    int at_ok = -1;
    for (int i = 0; i < 3 && at_ok != 0; i++) {
        if (i > 0) HAL_Delay(300);
        at_ok = at_cmd("AT", "OK", 2000);
    }
    if (at_ok != 0) {
        dbg_puts("[ESP] No AT response after 3 tries – check wiring PC6/PC7\r\n");
        return -1;
    }

    /* Check if already connected to an AP (ESP32 auto-reconnects from NVS).
       If so, skip the RST + CWJAP sequence entirely. */
    if (at_cmd("AT+CWJAP?", "+CWJAP:", 2000) == 0) {
        dbg_puts("[ESP] Already connected to WiFi (NVS)\r\n");
        return 0;
    }

    /* First-time connect: reset, set mode, join AP */
    dbg_puts("[ESP] Resetting...\r\n");
    esp_send("AT+RST\r\n");
    HAL_Delay(3000);
    { uint8_t dummy; while (HAL_UART_Receive(&huart6, &dummy, 1, 1) == HAL_OK) {} }
    if (at_cmd("AT", "OK", 3000) != 0) {
        dbg_puts("[ESP] Reset failed\r\n");
        return -1;
    }
    dbg_puts("[ESP] Reset OK\r\n");

    if (at_cmd("AT+CWMODE=1", "OK", 3000) != 0) {
        dbg_puts("[ESP] CWMODE failed\r\n");
        return -1;
    }

    /* Escape special AT chars: " \ , */
    char ssid_esc[128], pass_esc[128];
    at_escape(ssid, ssid_esc, sizeof(ssid_esc));
    at_escape(pass, pass_esc, sizeof(pass_esc));

    dbg_printf("[ESP] Joining '%s'...\r\n", ssid);
    char cmd[288];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid_esc, pass_esc);
    if (at_cmd(cmd, "WIFI GOT IP", 20000) != 0) {
        dbg_puts("[ESP] WiFi join failed\r\n");
        return -1;
    }

    dbg_puts("[ESP] WiFi connected, got IP\r\n");
    return 0;
}

int esp32_https_get(const char *host, const char *path,
                    char *resp_buf, uint16_t buf_len)
{
    char request[512];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
        path, host);
    if (req_len <= 0 || req_len >= (int)sizeof(request)) return -1;

    dbg_printf("[ESP] TCP connect -> %s:80\r\n", host);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, 80);
    if (at_cmd(cmd, "CONNECT", 10000) != 0) {
        dbg_puts("[ESP] TCP connect failed\r\n");
        return -1;
    }

    char cipsend[32];
    snprintf(cipsend, sizeof(cipsend), "AT+CIPSEND=%d", req_len);
    if (at_cmd(cipsend, ">", 5000) != 0) {
        at_cmd("AT+CIPCLOSE", NULL, 2000);
        dbg_puts("[ESP] CIPSEND failed\r\n");
        return -1;
    }

    dbg_puts("[ESP] Sending GET request...\r\n");
    if (esp_send(request) != 0) {
        at_cmd("AT+CIPCLOSE", NULL, 2000);
        return -1;
    }

    /* Read until the TCP connection closes ("CLOSED" from AT firmware).
     * "SEND OK" arrives quickly but the server reply (+IPD) can take
     * several seconds over WiFi, so we never break on idle alone —
     * only on CLOSED / ERROR / deadline. */
    uint32_t deadline = HAL_GetTick() + 15000;
    int      total    = 0;
    s_rx[0] = '\0';

    while (HAL_GetTick() < deadline) {
        int n = esp_recv(s_rx + total, (uint16_t)(sizeof(s_rx) - (uint16_t)total), 500);
        total += n;
        if (strstr(s_rx, "CLOSED") || strstr(s_rx, "ERROR")) break;
    }

    dbg_printf("[ESP] Received %d bytes total\r\n", total);

    /* Extract body after HTTP header blank line */
    const char *body = strstr(s_rx, "\r\n\r\n");
    if (!body) {
        body = strstr(s_rx, "\n\n");
        if (body) body += 2;
    } else {
        body += 4;
    }

    /* Advance to first JSON '{' */
    if (body) {
        const char *json_start = strchr(body, '{');
        if (json_start) body = json_start;
    }

    if (!body || *body == '\0') {
        dbg_puts("[ESP] No JSON body found\r\n");
        resp_buf[0] = '\0';
        return -1;
    }

    uint16_t n = 0;
    while (*body && n < buf_len - 1u) {
        if (strncmp(body, "\r\nCLOSED", 8) == 0) break;
        resp_buf[n++] = *body++;
    }
    resp_buf[n] = '\0';

    /* Log a short preview of the JSON */
    char preview[81];
    strncpy(preview, resp_buf, 80);
    preview[80] = '\0';
    dbg_printf("[ESP] JSON: %s...\r\n", preview);

    return (n > 0) ? 0 : -1;
}

int esp32_http_get_image(const char *host, uint16_t port, const char *path,
                         uint32_t dst_addr, uint32_t expected_bytes)
{
    char cmd[192];

    at_cmd("AT+CIPMUX=0",  "OK", 2000);

    /* Transparent passthrough: AT firmware forwards raw TCP bytes over UART */
    if (at_cmd("AT+CIPMODE=1", "OK", 2000) != 0) {
        dbg_puts("[IMG] AT+CIPMODE=1 failed\r\n");
        return -1;
    }

    /* Open TCP connection */
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u",
             host, (unsigned)port);
    if (at_cmd(cmd, "CONNECT", 10000) != 0) {
        dbg_printf("[IMG] TCP connect to %s:%u failed\r\n", host, (unsigned)port);
        at_cmd("AT+CIPMODE=0", "OK", 2000);
        return -1;
    }

    /* AT+CIPSEND (no length) → AT firmware replies ">" then enters passthrough */
    if (at_cmd("AT+CIPSEND", ">", 5000) != 0) {
        dbg_puts("[IMG] CIPSEND prompt not received\r\n");
        at_cmd("AT+CIPCLOSE", NULL, 2000);
        at_cmd("AT+CIPMODE=0", "OK", 2000);
        return -1;
    }

    /* Send HTTP/1.0 GET – server closes connection after the response,
     * which avoids chunked Transfer-Encoding and simplifies body detection */
    snprintf(cmd, sizeof(cmd),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, host);
    esp_send(cmd);
    dbg_printf("[IMG] GET %s HTTP/1.0 sent, streaming response…\r\n", path);

    /* ── Stream reception ────────────────────────────────────────────────
     * In transparent mode the AT firmware forwards raw TCP payload bytes
     * over UART with no +IPD framing overhead.
     * 1. Scan header bytes for the \r\n\r\n terminator.
     * 2. Once found, write every byte directly into SDRAM at dst_addr.
     * ─────────────────────────────────────────────────────────────────── */
    uint8_t *dst      = (uint8_t *)dst_addr;
    uint32_t body_len = 0;
    int      hdr_done = 0;
    uint8_t  tail[4]  = {0, 0, 0, 0};   /* rolling window for \r\n\r\n   */
    uint32_t deadline = HAL_GetTick() + 90000UL;  /* 90 s hard limit      */

    while (body_len < expected_bytes && HAL_GetTick() < deadline) {
        uint8_t b;
        /* 2 ms idle timeout – short enough to exit quickly when the
         * connection closes, but long enough not to spin-burn the CPU */
        if (HAL_UART_Receive(&huart6, &b, 1, 2) != HAL_OK)
            continue;

        if (!hdr_done) {
            tail[0] = tail[1]; tail[1] = tail[2]; tail[2] = tail[3]; tail[3] = b;
            if (tail[0] == '\r' && tail[1] == '\n' &&
                tail[2] == '\r' && tail[3] == '\n') {
                hdr_done = 1;
                /* No dbg_printf here – any blocking UART TX on USART1 causes
                 * USART6 receive overruns (ORE) and lost bytes at 115200 baud */
            }
        } else {
            dst[body_len++] = b;
            /* Do NOT log inside the tight receive loop: every blocking printf
             * on USART1 (~2-3 ms) causes USART6 byte loss via ORE overrun.
             * Progress is logged once after the loop completes. */
        }
    }

    dbg_printf("[IMG] Loop exit: %lu / %lu body bytes\r\n",
               body_len, expected_bytes);

    /* Exit transparent mode: 1-second silence, "+++", 1-second silence.
     * If the connection already closed the ESP32 may have exited passthrough
     * automatically; the flush inside at_cmd() handles any stale bytes. */
    HAL_Delay(1020);
    HAL_UART_Transmit(&huart6, (const uint8_t *)"+++", 3, 1000);
    HAL_Delay(1020);
    at_cmd("AT+CIPMODE=0", "OK", 3000);
    at_cmd("AT+CIPCLOSE",  "OK", 1000);   /* tidy up if still open */

    dbg_printf("[IMG] Done: %lu / %lu bytes received to 0x%08lX\r\n",
               body_len, expected_bytes, dst_addr);

    return (body_len >= expected_bytes) ? 0 : -1;
}
