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

/* ── SDRAM framebuffers (IS42S32400F, 8 MB @ 0xC0000000) ─────────────────── */
/* Each buffer: 480 × 272 × 2 = 261 120 B = 0x3FC00 B                        */
#define LCD_BUF_SNAP  0xC0000000UL   /* boot Flash snapshot (copy of windy_img[]) */
#define LCD_BUF_TEMP  0xC003FC00UL   /* temperature view                          */
#define LCD_BUF_HUM   0xC007F800UL   /* humidity view                             */
/* Total: 3 × 261 120 = 783 360 B ≈ 766 KB (well within 8 MB)                */

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

/* ── Buffer-switching API ────────────────────────────────────────────────── */

/**
 * @brief  Address currently displayed by LTDC (the "front" buffer).
 */
uint32_t windy_display_front_addr(void);

/**
 * @brief  Address of the buffer NOT currently displayed (safe to write into).
 */
uint32_t windy_display_back_addr(void);

/**
 * @brief  Point LTDC at an arbitrary SDRAM buffer (LCD_BUF_SNAP/TEMP/HUM).
 *         Uses HAL_LTDC_SetAddress() → update on next VSYNC, no tearing.
 */
void windy_display_set_addr(uint32_t addr);

/**
 * @brief  Swap between SNAP and TEMP buffers (legacy helper, not used by
 *         the dual-image main loop).
 */
void windy_display_flip(void);

#endif /* WINDY_DISPLAY_H */
