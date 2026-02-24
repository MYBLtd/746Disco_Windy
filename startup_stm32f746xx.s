/**
 * startup_stm32f746xx.s
 * Minimal Cortex-M7 startup for STM32F746xx
 * Toolchain: arm-none-eabi-gcc
 */

  .syntax unified
  .cpu cortex-m7
  .fpu fpv5-d16
  .thumb

/* ── Stack / Heap symbols from linker script ─────────────────────── */
  .global _estack
  .global __bss_start__
  .global __bss_end__
  .global _sidata
  .global _sdata
  .global _edata

/* ── Reset handler ───────────────────────────────────────────────── */
  .section .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function
Reset_Handler:
  /* Set stack pointer (redundant after vector table load, but safe) */
  ldr   sp, =_estack

  /* Copy .data from Flash to SRAM1 */
  ldr   r0, =_sdata
  ldr   r1, =_edata
  ldr   r2, =_sidata
  movs  r3, #0
  b     copy_data_loop
copy_data_body:
  ldr   r4, [r2, r3]
  str   r4, [r0, r3]
  adds  r3, r3, #4
copy_data_loop:
  adds  r4, r0, r3
  cmp   r4, r1
  bcc   copy_data_body

  /* Zero .bss */
  ldr   r2, =__bss_start__
  ldr   r4, =__bss_end__
  movs  r3, #0
  b     zero_bss_loop
zero_bss_body:
  str   r3, [r2]
  adds  r2, r2, #4
zero_bss_loop:
  cmp   r2, r4
  bcc   zero_bss_body

  /* Call SystemInit (CMSIS), then C runtime, then main */
  bl    SystemInit
  bl    __libc_init_array
  bl    main

  /* If main returns, loop forever */
loop_forever:
  b     loop_forever

  .size Reset_Handler, .-Reset_Handler

/* ── Default (weak) handler – infinite loop ─────────────────────── */
  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
  b     Default_Handler
  .size Default_Handler, .-Default_Handler

/* ── Weak aliases – override by defining the symbol elsewhere ───── */
  .macro WEAK_IRQ name
  .weak  \name
  .thumb_set \name, Default_Handler
  .endm

  WEAK_IRQ NMI_Handler
  WEAK_IRQ HardFault_Handler
  WEAK_IRQ MemManage_Handler
  WEAK_IRQ BusFault_Handler
  WEAK_IRQ UsageFault_Handler
  WEAK_IRQ SVC_Handler
  WEAK_IRQ DebugMon_Handler
  WEAK_IRQ PendSV_Handler
  WEAK_IRQ SysTick_Handler

  /* STM32F7 peripheral IRQs (subset – extend as needed) */
  WEAK_IRQ WWDG_IRQHandler
  WEAK_IRQ PVD_IRQHandler
  WEAK_IRQ TAMP_STAMP_IRQHandler
  WEAK_IRQ RTC_WKUP_IRQHandler
  WEAK_IRQ FLASH_IRQHandler
  WEAK_IRQ RCC_IRQHandler
  WEAK_IRQ EXTI0_IRQHandler
  WEAK_IRQ EXTI1_IRQHandler
  WEAK_IRQ EXTI2_IRQHandler
  WEAK_IRQ EXTI3_IRQHandler
  WEAK_IRQ EXTI4_IRQHandler
  WEAK_IRQ DMA1_Stream0_IRQHandler
  WEAK_IRQ DMA1_Stream1_IRQHandler
  WEAK_IRQ DMA1_Stream2_IRQHandler
  WEAK_IRQ DMA1_Stream3_IRQHandler
  WEAK_IRQ DMA1_Stream4_IRQHandler
  WEAK_IRQ DMA1_Stream5_IRQHandler
  WEAK_IRQ DMA1_Stream6_IRQHandler
  WEAK_IRQ ADC_IRQHandler
  WEAK_IRQ DMA1_Stream7_IRQHandler
  WEAK_IRQ LTDC_IRQHandler
  WEAK_IRQ LTDC_ER_IRQHandler
  WEAK_IRQ DMA2D_IRQHandler
  WEAK_IRQ SysTick_Handler

/* ── Interrupt vector table ─────────────────────────────────────── */
  .section .isr_vector,"a",%progbits
  .type   g_pfnVectors, %object
  .size   g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
  /* Core exceptions */
  .word _estack                    /* 0  Initial SP          */
  .word Reset_Handler              /* 1  Reset               */
  .word NMI_Handler                /* 2  NMI                 */
  .word HardFault_Handler          /* 3  Hard Fault          */
  .word MemManage_Handler          /* 4  MPU Fault           */
  .word BusFault_Handler           /* 5  Bus Fault           */
  .word UsageFault_Handler         /* 6  Usage Fault         */
  .word 0                          /* 7  reserved            */
  .word 0                          /* 8  reserved            */
  .word 0                          /* 9  reserved            */
  .word 0                          /* 10 reserved            */
  .word SVC_Handler                /* 11 SVCall              */
  .word DebugMon_Handler           /* 12 Debug Monitor       */
  .word 0                          /* 13 reserved            */
  .word PendSV_Handler             /* 14 PendSV              */
  .word SysTick_Handler            /* 15 SysTick             */

  /* STM32F746xx device IRQs (positions 0..134) */
  .word WWDG_IRQHandler            /* 0  WWDG                */
  .word PVD_IRQHandler             /* 1  PVD                 */
  .word TAMP_STAMP_IRQHandler      /* 2  Tamper / TimeStamp  */
  .word RTC_WKUP_IRQHandler        /* 3  RTC Wakeup          */
  .word FLASH_IRQHandler           /* 4  Flash               */
  .word RCC_IRQHandler             /* 5  RCC                 */
  .word EXTI0_IRQHandler           /* 6  EXTI0               */
  .word EXTI1_IRQHandler           /* 7  EXTI1               */
  .word EXTI2_IRQHandler           /* 8  EXTI2               */
  .word EXTI3_IRQHandler           /* 9  EXTI3               */
  .word EXTI4_IRQHandler           /* 10 EXTI4               */
  .word DMA1_Stream0_IRQHandler    /* 11 DMA1 Stream0        */
  .word DMA1_Stream1_IRQHandler    /* 12 DMA1 Stream1        */
  .word DMA1_Stream2_IRQHandler    /* 13 DMA1 Stream2        */
  .word DMA1_Stream3_IRQHandler    /* 14 DMA1 Stream3        */
  .word DMA1_Stream4_IRQHandler    /* 15 DMA1 Stream4        */
  .word DMA1_Stream5_IRQHandler    /* 16 DMA1 Stream5        */
  .word DMA1_Stream6_IRQHandler    /* 17 DMA1 Stream6        */
  .word ADC_IRQHandler             /* 18 ADC1, ADC2, ADC3    */
  .word Default_Handler            /* 19 CAN1 TX             */
  .word Default_Handler            /* 20 CAN1 RX0            */
  .word Default_Handler            /* 21 CAN1 RX1            */
  .word Default_Handler            /* 22 CAN1 SCE            */
  .word Default_Handler            /* 23 EXTI9_5             */
  .word Default_Handler            /* 24 TIM1 BRK / TIM9     */
  .word Default_Handler            /* 25 TIM1 UP / TIM10     */
  .word Default_Handler            /* 26 TIM1 TRG / TIM11    */
  .word Default_Handler            /* 27 TIM1 CC             */
  .word Default_Handler            /* 28 TIM2                */
  .word Default_Handler            /* 29 TIM3                */
  .word Default_Handler            /* 30 TIM4                */
  .word Default_Handler            /* 31 I2C1 EV             */
  .word Default_Handler            /* 32 I2C1 ER             */
  .word Default_Handler            /* 33 I2C2 EV             */
  .word Default_Handler            /* 34 I2C2 ER             */
  .word Default_Handler            /* 35 SPI1                */
  .word Default_Handler            /* 36 SPI2                */
  .word Default_Handler            /* 37 USART1              */
  .word Default_Handler            /* 38 USART2              */
  .word Default_Handler            /* 39 USART3              */
  .word Default_Handler            /* 40 EXTI15_10           */
  .word Default_Handler            /* 41 RTC Alarm           */
  .word Default_Handler            /* 42 OTG FS WKUP         */
  .word Default_Handler            /* 43 TIM8 BRK / TIM12    */
  .word Default_Handler            /* 44 TIM8 UP / TIM13     */
  .word Default_Handler            /* 45 TIM8 TRG / TIM14    */
  .word Default_Handler            /* 46 TIM8 CC             */
  .word DMA1_Stream7_IRQHandler    /* 47 DMA1 Stream7        */
  .word Default_Handler            /* 48 FMC                 */
  .word Default_Handler            /* 49 SDMMC1              */
  .word Default_Handler            /* 50 TIM5                */
  .word Default_Handler            /* 51 SPI3                */
  .word Default_Handler            /* 52 UART4               */
  .word Default_Handler            /* 53 UART5               */
  .word Default_Handler            /* 54 TIM6 / DAC1&2       */
  .word Default_Handler            /* 55 TIM7                */
  .word Default_Handler            /* 56 DMA2 Stream0        */
  .word Default_Handler            /* 57 DMA2 Stream1        */
  .word Default_Handler            /* 58 DMA2 Stream2        */
  .word Default_Handler            /* 59 DMA2 Stream3        */
  .word Default_Handler            /* 60 DMA2 Stream4        */
  .word Default_Handler            /* 61 ETH                 */
  .word Default_Handler            /* 62 ETH WKUP            */
  .word Default_Handler            /* 63 CAN2 TX             */
  .word Default_Handler            /* 64 CAN2 RX0            */
  .word Default_Handler            /* 65 CAN2 RX1            */
  .word Default_Handler            /* 66 CAN2 SCE            */
  .word Default_Handler            /* 67 OTG FS              */
  .word Default_Handler            /* 68 DMA2 Stream5        */
  .word Default_Handler            /* 69 DMA2 Stream6        */
  .word Default_Handler            /* 70 DMA2 Stream7        */
  .word Default_Handler            /* 71 USART6              */
  .word Default_Handler            /* 72 I2C3 EV             */
  .word Default_Handler            /* 73 I2C3 ER             */
  .word Default_Handler            /* 74 OTG HS EP1 OUT      */
  .word Default_Handler            /* 75 OTG HS EP1 IN       */
  .word Default_Handler            /* 76 OTG HS WKUP         */
  .word Default_Handler            /* 77 OTG HS              */
  .word Default_Handler            /* 78 DCMI                */
  .word Default_Handler            /* 79 reserved            */
  .word Default_Handler            /* 80 RNG                 */
  .word Default_Handler            /* 81 FPU                 */
  .word Default_Handler            /* 82 UART7               */
  .word Default_Handler            /* 83 UART8               */
  .word Default_Handler            /* 84 SPI4                */
  .word Default_Handler            /* 85 SPI5                */
  .word Default_Handler            /* 86 SPI6                */
  .word Default_Handler            /* 87 SAI1                */
  .word LTDC_IRQHandler            /* 88 LTDC                */
  .word LTDC_ER_IRQHandler         /* 89 LTDC Error          */
  .word DMA2D_IRQHandler           /* 90 DMA2D               */
  .word Default_Handler            /* 91 SAI2                */
  .word Default_Handler            /* 92 QuadSPI             */
  .word Default_Handler            /* 93 LPTIM1              */
  .word Default_Handler            /* 94 CEC                 */
  .word Default_Handler            /* 95 I2C4 EV             */
  .word Default_Handler            /* 96 I2C4 ER             */
  .word Default_Handler            /* 97 SPDIF RX            */
