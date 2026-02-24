/**
 * @file  windy_display.c
 * @brief Point LTDC at the embedded windy_img[] array and enable the display.
 *
 * The image lives in Flash (.rodata).  The LTDC's AHB master can read Flash
 * directly, so no copy to SDRAM is needed.  This keeps SDRAM free and avoids
 * a 261 KB memcpy on every boot.
 *
 * GPIO mapping (from STM32F746G-DISCO schematic):
 *
 *   Signal      Port/Pin    AF
 *   ----------- ----------- -----
 *   LTDC_CLK    PI14        AF14
 *   LTDC_HSYNC  PI10        AF14
 *   LTDC_VSYNC  PI9         AF14
 *   LTDC_DE     PI13 (**)   AF14   (** some revisions use PK7)
 *   LTDC_R0     PI15 / PJ0  AF14
 *   LTDC_R1     PJ1         AF14
 *   LTDC_R2     PJ2         AF14
 *   LTDC_R3     PJ3         AF14
 *   LTDC_R4     PJ4         AF14
 *   LTDC_R5     PJ5         AF14
 *   LTDC_R6     PJ6         AF14
 *   LTDC_R7     PJ7         AF14
 *   LTDC_G0     PJ8         AF14
 *   LTDC_G1     PJ9         AF14
 *   LTDC_G2     PJ10        AF14
 *   LTDC_G3     PJ11        AF14
 *   LTDC_G4     PJ12 / PB0  AF14/AF9
 *   LTDC_G5     PJ13 / PB1  AF14/AF9
 *   LTDC_G6     PJ14 / PK0  AF14
 *   LTDC_G7     PJ15 / PK1  AF14
 *   LTDC_B0     PE4  / PG14 AF14
 *   LTDC_B1     PJ13 (G5 alt, see schematic)
 *   LTDC_B2     PJ15 / PK0  AF14   (B-channel assignments vary by rev)
 *   LTDC_B3     PK4 (if present)
 *   LTDC_B4     PG12        AF9
 *   LTDC_B5     PK4         AF14
 *   LTDC_B6     PK5         AF14
 *   LTDC_B7     PK6         AF14
 *   LCD_DISP    PI12        GPIO output high
 *   LCD_BL_CTRL PK3         GPIO output high
 *
 * RGB565 timing (RK043FN48H-CT672B):
 *   HSYNC=41 ck  HBP=13  ActiveW=480  HFP=32
 *   VSYNC=10 ck  VBP=2   ActiveH=272  VFP=2
 *   LTDC register values = field_value − 1 in each case.
 */

#include "windy_display.h"
#include "windy_img.h"          /* generated: static const uint16_t windy_img[] */
#include "stm32f7xx_hal.h"

/* ── Timing (register values = actual − 1) ────────────────────────────────── */
#define HS   40U   /* HSYNC width − 1          */
#define HBP  52U   /* HSYNC + HBP − 1          */
#define HAW 532U   /* HSYNC + HBP + Width − 1  */
#define HTW 563U   /* total width − 1          */
#define VS    9U   /* VSYNC width − 1          */
#define VBP  11U   /* VSYNC + VBP − 1          */
#define VAH 283U   /* VSYNC + VBP + Height − 1 */
#define VTH 285U   /* total height − 1  (HAL typo: TotalHeigh) */

static LTDC_HandleTypeDef hltdc;

/* ── GPIO init ────────────────────────────────────────────────────────────── */
static void ltdc_gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF14_LTDC;

    /* PE4 – LTDC_B0 */
    g.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOE, &g);

    /* PG12 – LTDC_B4 (AF9) */
    g.Pin = GPIO_PIN_12; g.Alternate = GPIO_AF9_LTDC;
    HAL_GPIO_Init(GPIOG, &g);
    g.Alternate = GPIO_AF14_LTDC;

    /* PI9(VSYNC) PI10(HSYNC) PI13(DE) PI14(CLK) */
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_13 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOI, &g);

    /* PJ0-15 – R0-R7, G0-G7 (minus B channels on PK) */
    g.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  |
            GPIO_PIN_4  | GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7  |
            GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_11 |
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOJ, &g);

    /* PK0-2, PK4-6 – G6/G7/B5/B6/B7 (PK3 reserved for backlight) */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
            GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOK, &g);

    /* LCD_DISP – PI12: output high to enable the panel */
    g.Pin       = GPIO_PIN_12;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOI, &g);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);

    /* LCD_BL_CTRL – PK3: output high for full backlight */
    g.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &g);
    HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET);
}

/* ── LTDC init + layer configuration ─────────────────────────────────────── */
static int ltdc_init(void)
{
    __HAL_RCC_LTDC_CLK_ENABLE();

    hltdc.Instance                = LTDC;
    hltdc.Init.HorizontalSync     = HS;
    hltdc.Init.AccumulatedHBP     = HBP;
    hltdc.Init.AccumulatedActiveW = HAW;
    hltdc.Init.TotalWidth         = HTW;
    hltdc.Init.VerticalSync       = VS;
    hltdc.Init.AccumulatedVBP     = VBP;
    hltdc.Init.AccumulatedActiveH = VAH;
    hltdc.Init.TotalHeigh         = VTH;   /* STM32 HAL typo – intentional */

    hltdc.Init.HSPolarity         = LTDC_HSPOLARITY_AL;
    hltdc.Init.VSPolarity         = LTDC_VSPOLARITY_AL;
    hltdc.Init.DEPolarity         = LTDC_DEPOLARITY_AL;
    hltdc.Init.PCPolarity         = LTDC_PCPOLARITY_IPC;

    hltdc.Init.Backcolor.Red      = 0;
    hltdc.Init.Backcolor.Green    = 0;
    hltdc.Init.Backcolor.Blue     = 0;

    if (HAL_LTDC_Init(&hltdc) != HAL_OK)
        return -1;

    LTDC_LayerCfgTypeDef layer = {0};
    layer.WindowX0        = 0;
    layer.WindowX1        = WINDY_IMG_WIDTH;
    layer.WindowY0        = 0;
    layer.WindowY1        = WINDY_IMG_HEIGHT;
    layer.PixelFormat     = LTDC_PIXEL_FORMAT_RGB565;

    /* Point LTDC directly at the const array in Flash.
       The LTDC AHB master can read Flash – no SDRAM copy needed. */
    layer.FBStartAdress   = (uint32_t)windy_img;

    layer.Alpha           = 255;
    layer.Alpha0          = 0;
    layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    layer.ImageWidth      = WINDY_IMG_WIDTH;
    layer.ImageHeight     = WINDY_IMG_HEIGHT;
    layer.Backcolor.Red   = 0;
    layer.Backcolor.Green = 0;
    layer.Backcolor.Blue  = 0;

    if (HAL_LTDC_ConfigLayer(&hltdc, &layer, 0) != HAL_OK)
        return -2;

    return 0;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

int windy_display_show(void)
{
    ltdc_gpio_init();
    return ltdc_init();
}
