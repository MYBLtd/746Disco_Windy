/*
 * Linker script for STM32F746NG
 *   Flash : 0x08000000  1 MB
 *   DTCM  : 0x20000000  64 KB   (NOT DMA-accessible – no framebuffer here)
 *   SRAM1 : 0x20010000 240 KB
 *   SRAM2 : 0x2004C000  16 KB
 *
 *   External SDRAM 0xC0000000 8 MB is used as a run-time framebuffer; it is
 *   not described here because it is initialised and addressed directly in C.
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* top of SRAM1 – initial SP */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  DTCM  (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
  RAM   (rwx) : ORIGIN = 0x20010000, LENGTH = 240K
  SRAM2 (rwx) : ORIGIN = 0x2004C000, LENGTH = 16K
}

SECTIONS
{
  /* ── Interrupt vector table – must be first in Flash ── */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ── Code + read-only data ── */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ARM exception tables */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM        : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH

  /* C++ init/fini arrays */
  .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)); PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH
  .init_array    : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(.init_array*)); PROVIDE_HIDDEN(__init_array_end = .); } >FLASH
  .fini_array    : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(.fini_array*)); PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH

  /* ── Initialised data: stored in Flash, copied to SRAM1 at boot ── */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT>FLASH

  /* ── Zero-initialised data in SRAM1 ── */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* Heap grows up from end of .bss */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + 0x400;  /* 1 KB heap  */
    . = . + 0x800;  /* 2 KB stack */
    . = ALIGN(8);
  } >RAM

  /DISCARD/ : { libc.a(*) libm.a(*) libgcc.a(*) }
}
