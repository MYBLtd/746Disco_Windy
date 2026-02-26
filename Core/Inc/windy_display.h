/**
 * @file  windy_display.h
 * @brief LTDC display driver for STM32F746G-DISCO.
 *
 * Two operation modes:
 *
 * ── Static Flash mode (original) ────────────────────────────────────
 *   windy_display_show()
 *   LTDC points directly at the const windy_img[] array in Flash.
 *   No SDRAM needed; good for a one-shot baked image.
 *
 * ── Live SDRAM mode ──────────────────────────────────────────────────
 *   windy_display_init_sdram()   (call after SDRAM_Init())
 *   Copies windy_img[] Flash→SDRAM, reconfigures LTDC to 0xC0000000.
 *   Subsequent calls to windy_display_update_panel() overwrite the
 *   right-hand data panel with live weather values.
 */

#ifndef WINDY_DISPLAY_H
#define WINDY_DISPLAY_H

#include <stdint.h>
#include "weather_data.h"

/* Base address of IS42S32400F SDRAM (FMC Bank 1) */
#define LCD_FRAME_BUFFER  0xC0000000UL

/* Back framebuffer for double-buffering (261 120 B after the front buffer) */
#define LCD_BACK_BUFFER   (LCD_FRAME_BUFFER + 480U * 272U * 2U)

/**
 * @brief  Initialise LTDC + GPIO and show the embedded weather image
 *         directly from Flash.  Call once from main() – no SDRAM needed.
 * @retval 0 on success, non-zero on HAL error.
 */
int windy_display_show(void);

/**
 * @brief  Copy windy_img[] Flash→SDRAM, then point LTDC at the SDRAM
 *         framebuffer (LCD_FRAME_BUFFER).  The image is now writable.
 *         Must be called after SDRAM_Init().
 * @retval 0 on success, non-zero on HAL error.
 */
int windy_display_init_sdram(void);

/**
 * @brief  Overwrite the left data panel (x=0..149) with fresh weather values.
 *         Uses DMA2D for the background fill, CPU writes for the text.
 *         LTDC continues scanning from SDRAM with no interruption.
 * @param  wd  Pointer to current weather values (must not be NULL).
 */
void windy_display_update_panel(const WeatherData *wd);

/* ── Double-buffer API ───────────────────────────────────────────────────── */

/**
 * @brief  Address currently displayed by LTDC (the "front" buffer).
 */
uint32_t windy_display_front_addr(void);

/**
 * @brief  Address of the buffer NOT currently displayed (safe to write into).
 */
uint32_t windy_display_back_addr(void);

/**
 * @brief  Swap front and back buffers.
 *         Uses HAL_LTDC_SetAddress() which updates on the next VSYNC,
 *         preventing display tearing.
 */
void windy_display_flip(void);

#endif /* WINDY_DISPLAY_H */
