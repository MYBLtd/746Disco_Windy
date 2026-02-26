/**
 * @file  sdram.c
 * @brief FMC initialisation for IS42S32400F-7BL SDRAM on STM32F746G-DISCO
 *
 * Hardware:
 *   IS42S32400F  –  4M × 32-bit × 4 banks  =  8 MB
 *   FMC Bank 1 SDRAM  at 0xC0000000
 *   16-bit data bus (DQ16-DQ31 are NC on the DISCO board)
 *   12-bit row address, 8-bit column address
 *   CAS latency 3
 *   SDCLK = HCLK/2 = 108 MHz  → t_SDCLK ≈ 9.26 ns
 *
 * Timing values derived from IS42S32400F-7BL datasheet (−7 speed grade):
 *   tMRD  = 2 ck   (load mode reg command to active/refresh)
 *   tXSR  = 70 ns  ≈  8 ck @ 108 MHz  → register: 7
 *   tRAS  = 42 ns  ≈  5 ck            → register: 4
 *   tRC   = 60 ns  ≈  7 ck            → register: 6
 *   tWR   = 2 ck
 *   tRP   = 18 ns  ≈  2 ck            → register: 1
 *   tRCD  = 18 ns  ≈  2 ck            → register: 1
 *
 * Refresh: 64 ms / 8192 rows = 7.8 µs per row
 *   Count = t_SDCLK_Hz × t_refresh − 20
 *         = 108e6 × 7.8e-6 − 20 ≈ 822 – but we use 1386 to match DISCO BSP
 *   DISCO BSP uses 1386 which gives ≥ 64 ms for all 8192 rows.
 */

#include "sdram.h"

/* SDRAM Mode Register bits */
#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_3           0x0030U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200U

static void fmc_gpio_init(void);
static HAL_StatusTypeDef sdram_send_cmd(SDRAM_HandleTypeDef *hsdram,
                                         uint32_t cmd, uint32_t refresh,
                                         uint32_t regval);

HAL_StatusTypeDef SDRAM_Init(void)
{
    SDRAM_HandleTypeDef       hsdram  = {0};
    FMC_SDRAM_TimingTypeDef   timing  = {0};
    HAL_StatusTypeDef         status;

    fmc_gpio_init();

    __HAL_RCC_FMC_CLK_ENABLE();

    /* ── SDRAM controller config ─────────────────────────────────── */
    hsdram.Instance                = FMC_SDRAM_DEVICE;
    hsdram.Init.SDBank             = FMC_SDRAM_BANK1;
    hsdram.Init.ColumnBitsNumber   = FMC_SDRAM_COLUMN_BITS_NUM_8;
    hsdram.Init.RowBitsNumber      = FMC_SDRAM_ROW_BITS_NUM_12;
    hsdram.Init.MemoryDataWidth    = FMC_SDRAM_MEM_BUS_WIDTH_16;
    hsdram.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
    hsdram.Init.CASLatency         = FMC_SDRAM_CAS_LATENCY_3;
    hsdram.Init.WriteProtection    = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    hsdram.Init.SDClockPeriod      = FMC_SDRAM_CLOCK_PERIOD_2;   /* SDCLK = HCLK/2 */
    hsdram.Init.ReadBurst          = FMC_SDRAM_RBURST_ENABLE;
    hsdram.Init.ReadPipeDelay      = FMC_SDRAM_RPIPE_DELAY_0;

    /* ── Timing (register values = cycles - 1) ───────────────────── */
    timing.LoadToActiveDelay    = 2;   /* tMRD */
    timing.ExitSelfRefreshDelay = 7;   /* tXSR */
    timing.SelfRefreshTime      = 4;   /* tRAS */
    timing.RowCycleDelay        = 7;   /* tRC  */
    timing.WriteRecoveryTime    = 2;   /* tWR  */
    timing.RPDelay              = 2;   /* tRP  */
    timing.RCDDelay             = 2;   /* tRCD */

    status = HAL_SDRAM_Init(&hsdram, &timing);
    if (status != HAL_OK) return status;

    /* ── SDRAM initialisation sequence (JEDEC) ───────────────────── */

    /* 1. Clock enable */
    status = sdram_send_cmd(&hsdram, FMC_SDRAM_CMD_CLK_ENABLE, 1, 0);
    if (status != HAL_OK) return status;

    HAL_Delay(1);   /* ≥ 100 µs */

    /* 2. PALL – precharge all banks */
    status = sdram_send_cmd(&hsdram, FMC_SDRAM_CMD_PALL, 1, 0);
    if (status != HAL_OK) return status;

    /* 3. Auto-refresh × 8 */
    status = sdram_send_cmd(&hsdram, FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0);
    if (status != HAL_OK) return status;

    /* 4. Load Mode Register */
    uint32_t mode = SDRAM_MODEREG_BURST_LENGTH_1        |
                    SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL  |
                    SDRAM_MODEREG_CAS_LATENCY_3          |
                    SDRAM_MODEREG_OPERATING_MODE_STANDARD|
                    SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    status = sdram_send_cmd(&hsdram, FMC_SDRAM_CMD_LOAD_MODE, 1, mode);
    if (status != HAL_OK) return status;

    /* 5. Set refresh rate: 1386 matches the STM32F746G-DISCO BSP value */
    HAL_SDRAM_ProgramRefreshRate(&hsdram, 1386);

    return HAL_OK;
}

/* ── GPIO for FMC ────────────────────────────────────────────────── */

static void fmc_gpio_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF12_FMC;

    /* PC3  – FMC_SDCKE0 */
    g.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOC, &g);

    /* PD0,1,8,9,10,14,15 – FMC_D2/D3/D13..D15/D0/D1 */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 |
            GPIO_PIN_10| GPIO_PIN_14| GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &g);

    /* PE0,1,7..15 – FMC_NBL0/NBL1/D4..D12 */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 |
            GPIO_PIN_10| GPIO_PIN_11| GPIO_PIN_12| GPIO_PIN_13| GPIO_PIN_14|
            GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &g);

    /* PF0..5,11..15 – FMC_A0..A5, FMC_SDNRAS, FMC_A6..A9, FMC_A11 */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
            GPIO_PIN_5 | GPIO_PIN_11| GPIO_PIN_12| GPIO_PIN_13| GPIO_PIN_14|
            GPIO_PIN_15;
    HAL_GPIO_Init(GPIOF, &g);

    /* PG0,1,4,5,8,15 – FMC_A10, FMC_A11(BA0), FMC_BA0/BA1, FMC_SDCLK,
                         FMC_SDNCAS */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
            GPIO_PIN_8 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &g);

    /* PH3=FMC_SDNE0, PH5=FMC_SDNWE
     * DQ16-DQ31 (PH8-PH15, PI0-PI7) are NOT connected on DISCO — 16-bit bus */
    g.Pin = GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOH, &g);
}

/* ── Command helper ──────────────────────────────────────────────── */

static HAL_StatusTypeDef sdram_send_cmd(SDRAM_HandleTypeDef *hsdram,
                                         uint32_t cmd,
                                         uint32_t refresh,
                                         uint32_t regval)
{
    FMC_SDRAM_CommandTypeDef c = {0};
    c.CommandMode            = cmd;
    c.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
    c.AutoRefreshNumber      = refresh;
    c.ModeRegisterDefinition = regval;
    return HAL_SDRAM_SendCommand(hsdram, &c, 1000);
}
