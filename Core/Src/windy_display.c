/**
 * @file  windy_display.c
 * @brief LTDC + DMA2D driver for STM32F746G-DISCO weather display.
 *
 * GPIO mapping (from STM32F746G-DISCO schematic):
 *
 *   Signal      Port/Pin    AF
 *   ----------- ----------- -----
 *   LTDC_CLK    PI14        AF14
 *   LTDC_HSYNC  PI10        AF14
 *   LTDC_VSYNC  PI9         AF14
 *   LTDC_DE     PK7         AF14   ← real DE pin (not PI13)
 *   LTDC_R0     PI15        AF14
 *   LTDC_R1-R7  PJ0-PJ6     AF14
 *   LTDC_G0-G5  PJ8-PJ13    AF14
 *   LTDC_G6-G7  PK0-PK1     AF14
 *   LTDC_B0     PE4         AF14
 *   LTDC_B1-B3  PJ13-PJ15   AF14
 *   LTDC_B4     PG12        AF9
 *   LTDC_B5-B7  PK4-PK6     AF14
 *   LCD_DISP    PI12        GPIO high
 *   LCD_BL_CTRL PK3         GPIO high
 *
 * RGB565 timing (RK043FN48H-CT672B):
 *   HSYNC=41 ck  HBP=13  ActiveW=480  HFP=32
 *   VSYNC=10 ck  VBP=2   ActiveH=272  VFP=2
 *   LTDC register values = field_value − 1.
 *
 * Data panel occupies columns 300-479 (180 px wide), full height 272 px.
 */

#include "windy_display.h"
#include "windy_img.h"          /* static const uint16_t windy_img[] */
#include "font_draw.h"
#include "stm32f7xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ── Timing (register values = actual − 1) ─────────────────────────── */
/* Accumulated values: SSCR=HS, BPCR=HS+HBP-1, AWCR=HS+HBP+W-1, TWCR=total-1 */
/* Match official STM32F746G-DISCO BSP: HSYNC=41, HBP=13, W=480, HFP=32 */
#define HS   40U   /* 41 - 1                    */
#define HBP  53U   /* 41 + 13 - 1               */
#define HAW 533U   /* 41 + 13 + 480 - 1         */
#define HTW 565U   /* 41 + 13 + 480 + 32 - 1   */
#define VS    9U
#define VBP  11U
#define VAH 283U
#define VTH 285U   /* HAL typo: TotalHeigh */

/* ── Data panel geometry ─────────────────────────────────────────────── */
/* Left panel, matching windy_render.py PANEL_W=150                      */
#define PANEL_X      0
#define PANEL_W      150
#define PANEL_H      272

/* ── Colours (RGB565) ────────────────────────────────────────────────── */
#define COL_PANEL_BG  0x0010U   /* very dark navy  */
#define COL_WHITE     0xFFFFU
#define COL_CYAN      0x07FFU
#define COL_YELLOW    0xFFE0U
#define COL_LTGRAY    0xC618U

static LTDC_HandleTypeDef  hltdc;
static DMA2D_HandleTypeDef hdma2d;

/* Tracks which SDRAM buffer LTDC is currently scanning */
static uint32_t s_active = LCD_FRAME_BUFFER;

/* ── GPIO init (shared between show() and init_sdram()) ─────────────── */
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

    /* PI9(VSYNC) PI10(HSYNC) PI14(CLK) PI15(R0) */
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOI, &g);

    /* PJ0-15 – R0-R7, G0-G7 */
    g.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  |
            GPIO_PIN_4  | GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7  |
            GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_11 |
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOJ, &g);

    /* PK0-2, PK4-7  (PK7 = LTDC_DE) */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
            GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOK, &g);

    /* LCD_DISP – PI12 */
    g.Pin = GPIO_PIN_12; g.Mode = GPIO_MODE_OUTPUT_PP; g.Alternate = 0;
    HAL_GPIO_Init(GPIOI, &g);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);

    /* LCD_BL_CTRL – PK3 */
    g.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &g);
    HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET);
}

/* ── LTDC init, fb_addr = framebuffer start address ─────────────────── */
static int ltdc_init(uint32_t fb_addr)
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

    if (HAL_LTDC_Init(&hltdc) != HAL_OK) return -1;

    LTDC_LayerCfgTypeDef layer = {0};
    layer.WindowX0        = 0;
    layer.WindowX1        = WINDY_IMG_WIDTH;
    layer.WindowY0        = 0;
    layer.WindowY1        = WINDY_IMG_HEIGHT;
    layer.PixelFormat     = LTDC_PIXEL_FORMAT_RGB565;
    layer.FBStartAdress   = fb_addr;
    layer.Alpha           = 255;
    layer.Alpha0          = 0;
    layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    layer.ImageWidth      = WINDY_IMG_WIDTH;
    layer.ImageHeight     = WINDY_IMG_HEIGHT;
    layer.Backcolor.Red   = 0;
    layer.Backcolor.Green = 0;
    layer.Backcolor.Blue  = 0;

    if (HAL_LTDC_ConfigLayer(&hltdc, &layer, 0) != HAL_OK) return -2;

    return 0;
}

/* ── DMA2D init (used for panel background fill) ─────────────────────── */
static void dma2d_init(void)
{
    __HAL_RCC_DMA2D_CLK_ENABLE();
    hdma2d.Instance = DMA2D;
    /* Mode set per-operation; Init just enables the peripheral */
    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = 0;
    HAL_DMA2D_Init(&hdma2d);
}

/* ── DMA2D rectangle fill ────────────────────────────────────────────── */
static void fill_rect(uint32_t fb_addr,
                      int x, int y, int w, int h,
                      uint16_t colour)
{
    uint32_t dst = fb_addr + ((uint32_t)(y * 480 + x) * 2u);

    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = (uint32_t)(480 - w);
    HAL_DMA2D_Init(&hdma2d);

    HAL_DMA2D_Start(&hdma2d, (uint32_t)colour, dst,
                    (uint32_t)w, (uint32_t)h);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
}

/* ── Public API ──────────────────────────────────────────────────────── */

int windy_display_show(void)
{
    ltdc_gpio_init();
    return ltdc_init((uint32_t)windy_img);
}

int windy_display_init_sdram(void)
{
    /* Copy static map image from Flash to SDRAM framebuffer */
    memcpy((void *)LCD_FRAME_BUFFER, windy_img,
           WINDY_IMG_WIDTH * WINDY_IMG_HEIGHT * sizeof(uint16_t));

    s_active = LCD_FRAME_BUFFER;
    ltdc_gpio_init();
    dma2d_init();
    return ltdc_init(LCD_FRAME_BUFFER);
}

/* ── Double-buffer API ───────────────────────────────────────────────────── */

uint32_t windy_display_front_addr(void)
{
    return s_active;
}

uint32_t windy_display_back_addr(void)
{
    return (s_active == LCD_FRAME_BUFFER) ? LCD_BACK_BUFFER : LCD_FRAME_BUFFER;
}

void windy_display_flip(void)
{
    s_active = (s_active == LCD_FRAME_BUFFER) ? LCD_BACK_BUFFER : LCD_FRAME_BUFFER;
    HAL_LTDC_SetAddress(&hltdc, s_active, 0);
}

/* ── m/s → Beaufort number ───────────────────────────────────────────── */
static int wind_beaufort(float ms)
{
    static const float thr[] = {
        0.3f, 1.6f, 3.4f, 5.5f, 8.0f, 10.8f,
        13.9f, 17.2f, 20.8f, 24.5f, 28.5f, 32.7f
    };
    for (int i = 0; i < 12; i++)
        if (ms < thr[i]) return i;
    return 12;
}

void windy_display_update_panel(const WeatherData *wd)
{
    uint16_t *fb = (uint16_t *)LCD_FRAME_BUFFER;
    char line[32];
    int  x = PANEL_X + 4;

    /* ── 1. Clear panel ── */
    fill_rect(LCD_FRAME_BUFFER, PANEL_X, 0, PANEL_W, PANEL_H, COL_PANEL_BG);

    /* ── 2. Temperature – 3x scale (24×36 px per char, 6 chars = 144 px) ── */
    {
        int t10 = (wd->temperature >= 0.0f) ? (int)(wd->temperature * 10.0f)
                                            : -(int)(-wd->temperature * 10.0f);
        if (t10 < 0)
            snprintf(line, sizeof(line), "-%d.%dC", (-t10)/10, (-t10)%10);
        else
            snprintf(line, sizeof(line), "%d.%dC", t10/10, t10%10);
    }
    font_draw_string_scaled(fb, x, 4, line, COL_WHITE, COL_PANEL_BG, 3);

    /* ── 3. Weather description (1x) ── */
    font_draw_string(fb, x, 46, weather_code_str(wd->weather_code),
                     COL_CYAN, COL_PANEL_BG);

    /* ── 4. Humidity (1x) ── */
    snprintf(line, sizeof(line), "Hum: %d%%", wd->humidity);
    font_draw_string(fb, x, 62, line, COL_LTGRAY, COL_PANEL_BG);

    /* ── 5. Separator ── */
    for (int col = x; col < PANEL_X + PANEL_W - 4; col++)
        fb[78 * FB_WIDTH + col] = 0x2965u;

    /* ── 6. Wind Beaufort – 2x scale (16×24 px per char) ── */
    snprintf(line, sizeof(line), "Bft: %d", wind_beaufort(wd->wind_speed));
    font_draw_string_scaled(fb, x, 84, line, COL_YELLOW, COL_PANEL_BG, 2);

    /* ── 7. Wind speed m/s (1x, for reference) ── */
    {
        int w10 = (int)(wd->wind_speed * 10.0f);
        snprintf(line, sizeof(line), "%d.%d m/s", w10/10, w10%10);
    }
    font_draw_string(fb, x, 112, line, COL_YELLOW, COL_PANEL_BG);

    /* ── 8. Wind direction (1x) ── */
    static const char *dirs[] = {"N","NE","E","SE","S","SW","W","NW","N"};
    int dir_idx = ((wd->wind_dir + 22) / 45) & 7;
    snprintf(line, sizeof(line), "Dir: %s (%d)", dirs[dir_idx], wd->wind_dir);
    font_draw_string(fb, x, 128, line, COL_YELLOW, COL_PANEL_BG);
}
