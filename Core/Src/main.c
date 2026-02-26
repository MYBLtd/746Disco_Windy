/**
 * @file  main.c
 * @brief STM32F746G-DISCO – Windy.com live weather display (Pad A)
 *
 * Boot sequence:
 *   1. HAL_Init + SystemClock_Config (216 MHz, PLLSAI 9.6 MHz for LTDC)
 *   2. SDRAM_Init (IS42S32400F, 16-bit bus)
 *   3. windy_display_init_sdram – copy Flash snapshot → SDRAM, start LTDC
 *   4. esp32_init + esp32_connect_wifi
 *   5. Image download loop (10-minute refresh):
 *        esp32_http_get_image → back buffer → flip → delay
 *
 * The server (Debian 12) runs tools/windy_render.py every 10 minutes via
 * a systemd timer and serves windy.bin over HTTP on IMAGE_PORT.
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

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    dbg_uart_init();
    dbg_puts("\r\n=== Windy Weather Display (Pad A) ===\r\n");

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

    /* ── Image download loop ── */
    for (;;) {
        uint32_t back = windy_display_back_addr();
        show_status("Downloading...");
        dbg_printf("[IMG] Downloading to back buffer 0x%08lX\r\n", back);

        int rc = esp32_http_get_image(IMAGE_HOST, IMAGE_PORT, IMAGE_PATH,
                                      back, 480U * 272U * 2U);
        if (rc == 0) {
            windy_display_flip();
            show_status("OK");
            dbg_puts("[IMG] Display updated\r\n");
        } else {
            show_status("Download failed");
            dbg_puts("[IMG] Download failed, retrying in 30 s\r\n");
            HAL_Delay(30000UL);
            continue;
        }

        HAL_Delay(WEATHER_REFRESH_MS);
    }
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
