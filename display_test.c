/**
 * @file    display_test.c
 * @brief   Display test routines for STM32F746G-DISCO (HAL, no BSP)
 *
 * Panel:   RK043FN48H-CT672B  –  480x272, parallel RGB565, ~9 MHz pixel clock
 * LTDC pixel clock source: PLLSAI, configured externally in SystemClock_Config().
 *
 * Timing values below are taken directly from the panel datasheet:
 *   HSYNC=41  HBP=13  ActiveW=480  HFP=32
 *   VSYNC=10  VBP=2   ActiveH=272  VFP=2
 */

#include "display_test.h"
#include "stm32f7xx_hal.h"

/* ── Peripheral handles ──────────────────────────────────────────── */
static LTDC_HandleTypeDef  hltdc;
static DMA2D_HandleTypeDef hdma2d;

/* ── Internal helpers ────────────────────────────────────────────── */

/**
 * Fill a rectangular region of the framebuffer using DMA2D R2M mode.
 * colour is RGB565.  DMA2D register-to-memory uses 32-bit output, so we
 * need to write the colour replicated in a 32-bit word for RGB565 mode.
 */
static void fill_rect(uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h,
                       uint16_t colour)
{
    uint32_t dest = LCD_FRAME_BUFFER + (y * LCD_WIDTH + x) * LCD_BPP;

    hdma2d.Init.Mode         = DMA2D_R2M;
    hdma2d.Init.ColorMode    = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = LCD_WIDTH - w;

    HAL_DMA2D_Init(&hdma2d);
    /* R2M: colour argument is 32-bit; low 16 bits used for RGB565 */
    HAL_DMA2D_Start(&hdma2d, (uint32_t)colour, dest, w, h);
    HAL_DMA2D_PollForTransfer(&hdma2d, 1000);
}

/* Draw a 1-pixel horizontal line */
static void hline(uint32_t x, uint32_t y, uint32_t len, uint16_t colour)
{
    fill_rect(x, y, len, 1, colour);
}

/* Draw a 1-pixel vertical line */
static void vline(uint32_t x, uint32_t y, uint32_t len, uint16_t colour)
{
    uint16_t *fb = (uint16_t *)LCD_FRAME_BUFFER;
    for (uint32_t i = 0; i < len; i++)
        fb[(y + i) * LCD_WIDTH + x] = colour;
}

/* ── Peripheral initialisation ───────────────────────────────────── */

/**
 * Configure GPIO pins for LTDC (AF14).
 *
 * Pin mapping from the STM32F746G-DISCO schematic:
 *   PE4         – LTDC_B0
 *   PG12        – LTDC_B4
 *   PI9,10,13   – LTDC_VSYNC, LTDC_HSYNC, LTDC_DE
 *   PI14        – LTDC_CLK
 *   PJ0-7       – LTDC_R0-R3, LTDC_G0-G3
 *   PJ8-15      – LTDC_R4-R7 / LTDC_G4-G7 (varies by signal)
 *   PK0-5       – LTDC_B5-B7, LTDC_G5-G7
 *
 * LCD_DISP  → PI12 (active high, enable display)
 * LCD_BL_CTRL → PK3 (PWM / GPIO high for full brightness)
 *
 * Only the GPIO configuration is shown; SysClock / PLLSAI setup is in
 * SystemClock_Config() and is not repeated here.
 */
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

    /* PE4 – B0 */
    g.Pin = GPIO_PIN_4;
    HAL_GPIO_Init(GPIOE, &g);

    /* PG12 – B4 */
    g.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOG, &g);

    /* PI9(VSYNC) PI10(HSYNC) PI13(DE) PI14(CLK) */
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_13 | GPIO_PIN_14;
    HAL_GPIO_Init(GPIOI, &g);

    /* PJ0-15: R0-R7, G0-G5 */
    g.Pin = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_2  | GPIO_PIN_3  |
            GPIO_PIN_4  | GPIO_PIN_5  | GPIO_PIN_6  | GPIO_PIN_7  |
            GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_11 |
            GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOJ, &g);

    /* PK0-2(G6-G7,B5) PK4-6(B6-B7,LTDC_G5... check schematic) */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
            GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOK, &g);

    /* LCD_DISP – PI12, output high to enable panel */
    g.Pin       = GPIO_PIN_12;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Alternate = 0;
    HAL_GPIO_Init(GPIOI, &g);
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_12, GPIO_PIN_SET);

    /* LCD_BL_CTRL – PK3, output high for full backlight */
    g.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &g);
    HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET);
}

static int ltdc_init(void)
{
    __HAL_RCC_LTDC_CLK_ENABLE();

    /*
     * RK043FN48H timing (all register values = field_value - 1):
     *   HorizontalSync  = HSYNC - 1                      =  40
     *   AccumulatedHBP  = HSYNC + HBP - 1                =  52
     *   AccumulatedActiveW = HSYNC + HBP + Width - 1     = 532
     *   TotalWidth      = HSYNC + HBP + Width + HFP - 1  = 563
     *
     *   VerticalSync    = VSYNC - 1                      =   9
     *   AccumulatedVBP  = VSYNC + VBP - 1                =  11
     *   AccumulatedActiveH = VSYNC + VBP + Height - 1   = 283
     *   TotalHeight     = VSYNC + VBP + Height + VFP - 1= 285
     */
    hltdc.Instance                = LTDC;
    hltdc.Init.HorizontalSync     = 40;
    hltdc.Init.AccumulatedHBP     = 52;
    hltdc.Init.AccumulatedActiveW = 532;
    hltdc.Init.TotalWidth         = 563;
    hltdc.Init.VerticalSync       = 9;
    hltdc.Init.AccumulatedVBP     = 11;
    hltdc.Init.AccumulatedActiveH = 283;
    hltdc.Init.TotalHeigh         = 285;   /* STM32 HAL typo – kept as-is */

    hltdc.Init.HSPolarity         = LTDC_HSPOLARITY_AL;  /* active low */
    hltdc.Init.VSPolarity         = LTDC_VSPOLARITY_AL;
    hltdc.Init.DEPolarity         = LTDC_DEPOLARITY_AL;
    hltdc.Init.PCPolarity         = LTDC_PCPOLARITY_IPC; /* rising edge */

    hltdc.Init.Backcolor.Red      = 0;
    hltdc.Init.Backcolor.Green    = 0;
    hltdc.Init.Backcolor.Blue     = 0;

    if (HAL_LTDC_Init(&hltdc) != HAL_OK)
        return -1;

    /* Layer 0: full-screen, RGB565, framebuffer in SDRAM */
    LTDC_LayerCfgTypeDef layer = {0};
    layer.WindowX0        = 0;
    layer.WindowX1        = LCD_WIDTH;
    layer.WindowY0        = 0;
    layer.WindowY1        = LCD_HEIGHT;
    layer.PixelFormat     = LTDC_PIXEL_FORMAT_RGB565;
    layer.FBStartAdress   = LCD_FRAME_BUFFER;
    layer.Alpha           = 255;
    layer.Alpha0          = 0;
    layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
    layer.ImageWidth      = LCD_WIDTH;
    layer.ImageHeight     = LCD_HEIGHT;
    layer.Backcolor.Red   = 0;
    layer.Backcolor.Green = 0;
    layer.Backcolor.Blue  = 0;

    if (HAL_LTDC_ConfigLayer(&hltdc, &layer, 0) != HAL_OK)
        return -2;

    return 0;
}

static void dma2d_init(void)
{
    __HAL_RCC_DMA2D_CLK_ENABLE();
    hdma2d.Instance = DMA2D;
    /* Detailed mode set per-call in fill_rect() */
}

/* ── Public API ──────────────────────────────────────────────────── */

int display_test_init(void)
{
    ltdc_gpio_init();
    dma2d_init();
    return ltdc_init();
}

/* ── Test patterns ───────────────────────────────────────────────── */

void display_test_solid(uint16_t colour)
{
    fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, colour);
}

void display_test_colour_bars(void)
{
    /*
     * Eight SMPTE-style bars, each LCD_WIDTH/8 = 60 px wide.
     * Order: White  Yellow  Cyan  Green  Magenta  Red  Blue  Black
     */
    static const uint16_t bars[8] = {
        COLOR_WHITE,
        COLOR_YELLOW,
        COLOR_CYAN,
        COLOR_GREEN,
        COLOR_MAGENTA,
        COLOR_RED,
        COLOR_BLUE,
        COLOR_BLACK,
    };
    uint32_t bar_w = LCD_WIDTH / 8;   /* 60 px */

    for (uint32_t i = 0; i < 8; i++)
        fill_rect(i * bar_w, 0, bar_w, LCD_HEIGHT, bars[i]);

    /* Fill any remainder (480 % 8 == 0, so none here) */
}

void display_test_checkerboard(void)
{
    const uint32_t sq = 32; /* square size in pixels */

    for (uint32_t row = 0; row < LCD_HEIGHT; row += sq) {
        uint32_t h = (row + sq > LCD_HEIGHT) ? LCD_HEIGHT - row : sq;
        for (uint32_t col = 0; col < LCD_WIDTH; col += sq) {
            uint32_t w     = (col + sq > LCD_WIDTH) ? LCD_WIDTH - col : sq;
            uint16_t colour = ((row / sq + col / sq) & 1) ? COLOR_WHITE : COLOR_BLACK;
            fill_rect(col, row, w, h, colour);
        }
    }
}

void display_test_gradient(void)
{
    /*
     * Three horizontal gradient bands stacked vertically:
     *   Top third   – red ramp    (black → red)
     *   Middle third – green ramp  (black → green)
     *   Bottom third – blue ramp   (black → blue)
     *
     * Each vertical column is one pixel wide and spans one band.
     * fill_rect(col, y_start, 1, band_h, colour) is 272 calls but kept
     * simple and readable; use DMA2D M2M with format conversion for speed.
     */
    uint32_t band_h = LCD_HEIGHT / 3;   /* ~90 px */

    for (uint32_t x = 0; x < LCD_WIDTH; x++) {
        uint8_t  intensity = (uint8_t)((x * 255U) / (LCD_WIDTH - 1U));
        uint16_t c_red     = RGB565(intensity, 0, 0);
        uint16_t c_grn     = RGB565(0, intensity, 0);
        uint16_t c_blu     = RGB565(0, 0, intensity);

        fill_rect(x, 0,            1, band_h, c_red);
        fill_rect(x, band_h,       1, band_h, c_grn);
        fill_rect(x, band_h * 2U, 1, band_h + (LCD_HEIGHT % 3U), c_blu);
    }
}

void display_test_grid(void)
{
    /* Black background */
    fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_BLACK);

    const uint32_t step = 32;

    /* Horizontal white lines */
    for (uint32_t y = 0; y < LCD_HEIGHT; y += step)
        hline(0, y, LCD_WIDTH, COLOR_WHITE);

    /* Vertical white lines (CPU pixel-write; DMA2D R2M cannot stride) */
    for (uint32_t x = 0; x < LCD_WIDTH; x += step)
        vline(x, 0, LCD_HEIGHT, COLOR_WHITE);

    /* Mark screen centre with a contrasting cross */
    hline(LCD_WIDTH / 2 - 20, LCD_HEIGHT / 2, 40, COLOR_RED);
    vline(LCD_WIDTH / 2, LCD_HEIGHT / 2 - 20, 40, COLOR_RED);
}

void display_test_run_all(void)
{
    typedef struct { void (*fn)(void); const char *name; } Pattern;

    /* Solid-colour flash: red / green / blue to catch per-channel wiring faults */
    display_test_solid(COLOR_RED);    HAL_Delay(800);
    display_test_solid(COLOR_GREEN);  HAL_Delay(800);
    display_test_solid(COLOR_BLUE);   HAL_Delay(800);
    display_test_solid(COLOR_WHITE);  HAL_Delay(800);
    display_test_solid(COLOR_BLACK);  HAL_Delay(800);

    static const Pattern patterns[] = {
        { display_test_colour_bars,  "Colour bars"   },
        { display_test_checkerboard, "Checkerboard"  },
        { display_test_gradient,     "Gradient"      },
        { display_test_grid,         "Grid"          },
    };

    while (1) {
        for (uint32_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
            patterns[i].fn();
            HAL_Delay(2000);
        }
    }
}
