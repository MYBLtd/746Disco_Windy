##########################################################################
# Makefile – STM32F746G-DISCO display test
#
# Usage:
#   make              # build
#   make flash        # build + flash via ST-Link (st-flash)
#   make flash FLASH_TOOL=openocd   # flash via OpenOCD instead
#   make clean
#
# Prerequisites:
#   arm-none-eabi-gcc (tested with 14.x)
#   git submodule update --init --depth 1    (first clone only)
#   st-flash  OR  openocd   (for 'make flash')
##########################################################################

TARGET     = display_test
BUILD_DIR  = build

# ── Driver paths (git submodules inside Drivers/) ─────────────────
HAL_DIR    = Drivers/STM32F7xx_HAL_Driver
CMSIS_DEV  = Drivers/CMSIS/Device/ST/STM32F7xx
CMSIS_CORE = Drivers/CMSIS/Include

# ── Toolchain ─────────────────────────────────────────────────────
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# ── CPU / FPU flags ───────────────────────────────────────────────
CPU     = -mcpu=cortex-m7
FPU     = -mfpu=fpv5-d16 -mfloat-abi=hard
ARCH    = $(CPU) -mthumb $(FPU)

# ── Defines ───────────────────────────────────────────────────────
DEFS    = -DSTM32F746xx -DUSE_HAL_DRIVER

# ── Include paths ─────────────────────────────────────────────────
INCS    = \
    -ICore/Inc \
    -I. \
    -I$(HAL_DIR)/Inc \
    -I$(HAL_DIR)/Inc/Legacy \
    -I$(CMSIS_DEV)/Include \
    -I$(CMSIS_CORE)

# ── Application source files ──────────────────────────────────────
APP_C_SRCS = \
    Core/Src/main.c \
    Core/Src/windy_display.c \
    Core/Src/stm32f7xx_it.c \
    Core/Src/sdram.c \
    Core/Src/esp32_at.c \
    Core/Src/weather_data.c \
    Core/Src/font_draw.c \
    Core/Src/dbg_uart.c \
    display_test.c

# ── HAL source files (only what we use) ───────────────────────────
HAL_C_SRCS = \
    $(HAL_DIR)/Src/stm32f7xx_hal.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_cortex.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_rcc.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_rcc_ex.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_gpio.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_pwr.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_pwr_ex.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_flash.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_flash_ex.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_dma.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_ltdc.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_dma2d.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_sdram.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_uart.c \
    $(HAL_DIR)/Src/stm32f7xx_hal_uart_ex.c \
    $(HAL_DIR)/Src/stm32f7xx_ll_fmc.c \
    $(CMSIS_DEV)/Source/Templates/system_stm32f7xx.c

# ── Assembly startup ──────────────────────────────────────────────
AS_SRCS = startup_stm32f746xx.s

# ── All objects ───────────────────────────────────────────────────
ALL_C_SRCS = $(APP_C_SRCS) $(HAL_C_SRCS)
OBJS  = $(addprefix $(BUILD_DIR)/,$(notdir $(ALL_C_SRCS:.c=.o)))
OBJS += $(addprefix $(BUILD_DIR)/,$(notdir $(AS_SRCS:.s=.o)))

# Make each .c findable regardless of depth
vpath %.c $(sort $(dir $(ALL_C_SRCS)))
vpath %.s $(sort $(dir $(AS_SRCS)))

# ── Compiler / linker flags ───────────────────────────────────────
CFLAGS  = $(ARCH) $(DEFS) $(INCS) \
          -std=c11 -Wall -Wextra \
          -fdata-sections -ffunction-sections \
          -Os -g3

ASFLAGS = $(ARCH) $(DEFS) $(INCS) -g3

LDSCRIPT = STM32F746NGHx_FLASH.ld
LDFLAGS  = $(ARCH) \
           -T$(LDSCRIPT) \
           -specs=nano.specs \
           -specs=nosys.specs \
           -Wl,--gc-sections \
           -Wl,-u,_printf_float \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
           -lc -lm

# ── Build targets ─────────────────────────────────────────────────
.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf
	@$(SIZE) $<

$(BUILD_DIR)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	@echo "  LD  $@"
	@$(CC) $(OBJS) $(LDFLAGS) -o $@
	@$(OBJCOPY) -O ihex  $@ $(BUILD_DIR)/$(TARGET).hex
	@$(OBJCOPY) -O binary --gap-fill 0xFF $@ $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "  CC  $<"
	@$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@echo "  AS  $<"
	@$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned."

# ── Flash ──────────────────────────────────────────────────────────
# Default tool: st-flash (stlink).  Override with FLASH_TOOL=openocd.
FLASH_TOOL ?= st-flash
FLASH_ADDR  = 0x08000000

.PHONY: flash

flash: $(BUILD_DIR)/$(TARGET).elf
ifeq ($(FLASH_TOOL),openocd)
	# Connect to the already-running OpenOCD (tunnelled to localhost:13333)
	gdb-multiarch -q $< \
	  -ex "set architecture armv7e-m" \
	  -ex "target extended-remote localhost:13333" \
	  -ex "monitor reset init" \
	  -ex "load" \
	  -ex "monitor reset run" \
	  -ex "detach" \
	  -ex "quit"
else
	st-flash --reset write $(BUILD_DIR)/$(TARGET).bin $(FLASH_ADDR)
endif
