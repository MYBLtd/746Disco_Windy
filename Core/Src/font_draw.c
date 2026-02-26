/**
 * @file  font_draw.c
 * @brief Bitmap text rendering to SDRAM RGB565 framebuffer.
 */

#include "font_draw.h"
#include "font8x12.h"
#include <stddef.h>

void font_draw_char(uint16_t *fb, int x, int y, char c,
                    uint16_t fg, uint16_t bg)
{
    if (c < (char)FONT_FIRST || c > (char)FONT_LAST) c = '?';

    const uint8_t *glyph = font8x12[(uint8_t)c - FONT_FIRST];

    for (int row = 0; row < (int)FONT_H; row++) {
        int py = y + row;
        if (py < 0 || py >= (int)FB_HEIGHT) continue;

        uint8_t bits = glyph[row];
        for (int col = 0; col < (int)FONT_W; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)FB_WIDTH) continue;
            fb[py * FB_WIDTH + px] = (bits & (0x01u << col)) ? fg : bg;
        }
    }
}

void font_draw_string(uint16_t *fb, int x, int y, const char *s,
                      uint16_t fg, uint16_t bg)
{
    if (!s) return;
    int cx = x;
    while (*s) {
        font_draw_char(fb, cx, y, *s, fg, bg);
        cx += (int)FONT_W;
        s++;
    }
}

void font_draw_string_scaled(uint16_t *fb, int x, int y, const char *s,
                             uint16_t fg, uint16_t bg, int scale)
{
    if (!s || scale <= 0) return;
    if (scale == 1) { font_draw_string(fb, x, y, s, fg, bg); return; }

    int cx = x;
    while (*s) {
        char c = *s;
        if (c < (char)FONT_FIRST || c > (char)FONT_LAST) c = '?';
        const uint8_t *glyph = font8x12[(uint8_t)c - FONT_FIRST];

        for (int row = 0; row < (int)FONT_H; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < (int)FONT_W; col++) {
                uint16_t pix = (bits & (0x01u << col)) ? fg : bg;
                for (int sy = 0; sy < scale; sy++) {
                    int py = y + row * scale + sy;
                    if (py < 0 || py >= (int)FB_HEIGHT) continue;
                    for (int sx = 0; sx < scale; sx++) {
                        int px = cx + col * scale + sx;
                        if (px < 0 || px >= (int)FB_WIDTH) continue;
                        fb[py * FB_WIDTH + px] = pix;
                    }
                }
            }
        }
        cx += (int)FONT_W * scale;
        s++;
    }
}
