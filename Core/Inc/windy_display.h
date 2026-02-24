/**
 * @file  windy_display.h
 * @brief Display the Windy-style weather image on the STM32F746G-DISCO LCD.
 *
 * The image is embedded in Flash as a const uint16_t array (windy_img[]) by
 * the auto-generated Core/Inc/windy_img.h.  The LTDC layer is pointed
 * directly at the Flash address – no SDRAM copy required for the image.
 *
 * Usage:
 *   1. Run  tools/windy_render.py   → generates Core/Inc/windy_img.h
 *   2. Run  make                    → firmware embeds the image
 *   3. Flash & boot                 → display shows the weather map
 */

#ifndef WINDY_DISPLAY_H
#define WINDY_DISPLAY_H

#include <stdint.h>

/**
 * @brief  Initialise LTDC + GPIO and show the embedded weather image.
 *         Blocks until the image is visible (LTDC started).
 *         Call once from main() after SystemClock_Config().
 * @retval 0 on success, non-zero on HAL error.
 */
int windy_display_show(void);

#endif /* WINDY_DISPLAY_H */
