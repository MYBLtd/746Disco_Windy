/**
 * @file  main.c
 * @brief STM32F746G-DISCO – IoT sensor tile display (Pad A)
 *
 * Boot sequence:
 *   1. HAL_Init + SystemClock_Config (216 MHz, PLLSAI 9.6 MHz for LTDC)
 *   2. SDRAM_Init (IS42S32400F, 16-bit bus)
 *   3. windy_display_init_sdram – copy Flash snapshot → LCD_BUF_SNAP, start LTDC
 *   4. esp32_init + esp32_connect_wifi
 *   5. Download windy_temp.bin → LCD_BUF_TEMP (show snap meanwhile)
 *   6. Download windy_hum.bin  → LCD_BUF_HUM  (show temp meanwhile)
 *   7. Alternation loop: flip T/RH every SENSOR_FLIP_MS;
 *      re-download both every WEATHER_REFRESH_MS without display gaps.
 *
 * The server (Debian 12) runs tools/windy_render.py every 10 minutes via
 * a systemd timer and serves windy_temp.bin + windy_hum.bin over HTTP.
 * Edit IMAGE_HOST in weather_config.h to point at your server.
 */

#include "main.h"
#include "sdram.h"
#include "windy_display.h"
#include "esp32_at.h"
#include "weather_config.h"
#include "font_draw.h"
#include "dbg_uart.h"

static void SystemClock_Config(void);
static void show_status(const char *msg);
static int  download(const char *path, uint32_t buf);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    dbg_uart_init();
    dbg_puts("\r\n=== Windy Sensor Tile Display (Pad A) ===\r\n");

    dbg_puts("[BOOT] SDRAM init...\r\n");
    if (SDRAM_Init() != HAL_OK) {
        dbg_puts("[BOOT] SDRAM FAILED\r\n");
        Error_Handler();
    }

    dbg_puts("[BOOT] Display init...\r\n");
    if (windy_display_init_sdram() != 0) {
        dbg_puts("[BOOT] Display FAILED\r\n");
        Error_Handler();
    }
    dbg_puts("[BOOT] Display OK – showing Flash snapshot\r\n");

    /* ── WiFi connect ── */
    show_status("ESP32 init...");
    if (esp32_init() != 0) {
        show_status("UART init failed!");
        dbg_puts("[BOOT] ESP32 UART init failed\r\n");
    } else {
        show_status("Connecting WiFi...");
        if (esp32_connect_wifi(WIFI_SSID, WIFI_PASS) != 0) {
            show_status("WiFi failed");
            dbg_puts("[BOOT] WiFi connect failed\r\n");
        } else {
            dbg_puts("[BOOT] WiFi OK\r\n");
        }
    }

    /* ── Boot: show snapshot while downloading both views ── */
    windy_display_set_addr(LCD_BUF_SNAP);
    show_status("DL temp...");
    download(IMAGE_PATH_TEMP, LCD_BUF_TEMP);

    windy_display_set_addr(LCD_BUF_TEMP);
    show_status("DL hum...");
    download(IMAGE_PATH_HUM, LCD_BUF_HUM);

    /* ── Alternation + 10-minute refresh loop ── */
    int      showing_temp = 1;
    uint32_t last_fetch   = HAL_GetTick();
    uint32_t last_flip    = HAL_GetTick();
    windy_display_set_addr(LCD_BUF_TEMP);
    show_status("OK");

    for (;;) {
        HAL_Delay(500UL);

        /* Flip between T and RH every SENSOR_FLIP_MS */
        if (HAL_GetTick() - last_flip >= SENSOR_FLIP_MS) {
            showing_temp = !showing_temp;
            windy_display_set_addr(showing_temp ? LCD_BUF_TEMP : LCD_BUF_HUM);
            last_flip = HAL_GetTick();
        }

        /* Re-download both images every WEATHER_REFRESH_MS */
        if (HAL_GetTick() - last_fetch >= WEATHER_REFRESH_MS) {
            /* Download temp into LCD_BUF_TEMP while showing hum */
            windy_display_set_addr(LCD_BUF_HUM);
            show_status("DL temp...");
            download(IMAGE_PATH_TEMP, LCD_BUF_TEMP);

            /* Download hum into LCD_BUF_HUM while showing temp */
            windy_display_set_addr(LCD_BUF_TEMP);
            show_status("DL hum...");
            download(IMAGE_PATH_HUM, LCD_BUF_HUM);

            showing_temp  = 1;
            last_fetch    = HAL_GetTick();
            last_flip     = HAL_GetTick();
            windy_display_set_addr(LCD_BUF_TEMP);
            show_status("OK");
        }
    }
}

/* ── Download helper ────────────────────────────────────────────────────── */
static int download(const char *path, uint32_t buf)
{
    dbg_printf("[IMG] %s → 0x%08lX\r\n", path, buf);
    int rc = esp32_http_get_image(IMAGE_HOST, IMAGE_PORT, path,
                                  buf, 480U * 272U * 2U);
    if (rc != 0)
        dbg_printf("[IMG] FAILED rc=%d\r\n", rc);
    return rc;
}

/* ── Status line: bottom-left corner of the currently displayed buffer ── */
static void show_status(const char *msg)
{
    uint16_t *fb = (uint16_t *)windy_display_front_addr();
    int y = 272 - 12 - 2;
    for (int row = y; row < 272; row++)
        for (int col = 0; col < 150; col++)
            fb[row * 480 + col] = 0x0010u;
    font_draw_string(fb, 4, y, msg, 0x07FFu, 0x0010u);
}

/* ── Clock configuration (same as before) ────────────────────────────── */
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

    PeriphClkInit.PeriphClockSelection  = RCC_PERIPHCLK_LTDC;
    PeriphClkInit.PLLSAI.PLLSAIN        = 192;
    PeriphClkInit.PLLSAI.PLLSAIR        = 5;
    PeriphClkInit.PLLSAIDivR            = RCC_PLLSAIDIVR_4;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        Error_Handler();
}

/* ── HAL callbacks ────────────────────────────────────────────────────── */

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
