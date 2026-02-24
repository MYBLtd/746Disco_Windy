/**
 * @file    display_test.h
 * @brief   Display test routines for STM32F746G-DISCO
 *
 * Hardware: RK043FN48H-CT672B TFT (480x272, RGB565 via LTDC)
 *
 * Prerequisites:
 *   - System clock configured with PLLSAI for LTDC pixel clock (~9 MHz)
 *   - External SDRAM initialized (FMC, IS42S32400F at 0xC0000000)
 *     Framebuffer requires 480*272*2 = 261,120 bytes — too large for internal SRAM
 *   - GPIO clocks and alternate functions for LTDC pins enabled
 *
 * Usage:
 *   display_test_init();
 *   display_test_run_all();   // cycles through all patterns
 */

#ifndef DISPLAY_TEST_H
#define DISPLAY_TEST_H

#include <stdint.h>

/* ── Framebuffer ─────────────────────────────────────────────────── */
#define LCD_FRAME_BUFFER    0xC0000000UL   /* External SDRAM */
#define LCD_WIDTH           480U
#define LCD_HEIGHT          272U
#define LCD_BPP             2U             /* RGB565 = 2 bytes/pixel */

/* ── RGB565 colour helpers ───────────────────────────────────────── */
#define RGB565(r, g, b) \
    ((uint16_t)(((r) & 0xF8u) << 8 | ((g) & 0xFCu) << 3 | ((b) & 0xF8u) >> 3))

#define COLOR_BLACK   RGB565(  0,   0,   0)
#define COLOR_WHITE   RGB565(255, 255, 255)
#define COLOR_RED     RGB565(255,   0,   0)
#define COLOR_GREEN   RGB565(  0, 255,   0)
#define COLOR_BLUE    RGB565(  0,   0, 255)
#define COLOR_YELLOW  RGB565(255, 255,   0)
#define COLOR_CYAN    RGB565(  0, 255, 255)
#define COLOR_MAGENTA RGB565(255,   0, 255)
#define COLOR_ORANGE  RGB565(255, 165,   0)

/* ── Public API ──────────────────────────────────────────────────── */

/**
 * @brief  Initialise LTDC and DMA2D peripherals.
 *         Call once after system clock and SDRAM are ready.
 * @retval 0 on success, non-zero on HAL error.
 */
int display_test_init(void);

/** Fill the entire screen with a solid colour. */
void display_test_solid(uint16_t colour);

/**
 * @brief  Draw eight vertical SMPTE-style colour bars:
 *         White / Yellow / Cyan / Green / Magenta / Red / Blue / Black
 */
void display_test_colour_bars(void);

/** Alternating black-and-white checkerboard (32x32 px squares). */
void display_test_checkerboard(void);

/** Horizontal red→green→blue gradient across the full width. */
void display_test_gradient(void);

/** White horizontal + vertical grid lines every 32 pixels on black. */
void display_test_grid(void);

/**
 * @brief  Run all patterns in sequence, pausing ~1 s between each.
 *         Loops indefinitely — suitable as a standalone production test.
 */
void display_test_run_all(void);

#endif /* DISPLAY_TEST_H */
