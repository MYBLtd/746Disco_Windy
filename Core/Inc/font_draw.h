/**
 * @file  font_draw.h
 * @brief Bitmap text rendering into an RGB565 framebuffer.
 *
 * Uses font8x12.h – 8×12 monospace, all printable ASCII.
 * CPU pixel writes to the SDRAM framebuffer (DMA2D cannot handle stride).
 */

#ifndef FONT_DRAW_H
#define FONT_DRAW_H

#include <stdint.h>

#define FB_WIDTH  480U
#define FB_HEIGHT 272U

/**
 * @brief  Render a single character at (x, y).
 * @param  fb    Pointer to start of 480×272 RGB565 framebuffer.
 * @param  x     Left pixel column (0-based).
 * @param  y     Top pixel row (0-based).
 * @param  c     ASCII character to draw.
 * @param  fg    Foreground colour (RGB565).
 * @param  bg    Background colour (RGB565), drawn for 0-bits.
 */
void font_draw_char(uint16_t *fb, int x, int y, char c,
                    uint16_t fg, uint16_t bg);

/**
 * @brief  Render a NUL-terminated string starting at (x, y).
 *         Characters advance by FONT_W pixels each.  No wrapping.
 */
void font_draw_string(uint16_t *fb, int x, int y, const char *s,
                      uint16_t fg, uint16_t bg);

/**
 * @brief  Render a NUL-terminated string with integer pixel scaling.
 *         Each font pixel is drawn as scale×scale screen pixels.
 *         scale=1 is identical to font_draw_string.
 */
void font_draw_string_scaled(uint16_t *fb, int x, int y, const char *s,
                             uint16_t fg, uint16_t bg, int scale);

#endif /* FONT_DRAW_H */
