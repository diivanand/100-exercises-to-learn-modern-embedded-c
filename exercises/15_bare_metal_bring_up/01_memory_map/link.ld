/* Linker script for the STM32L476RG (NUCLEO-L476RG board).
 *
 * This file answers one question: WHERE DOES EVERYTHING GO. The C language
 * says nothing about it; on the desktop the OS loader decides; here, you do.
 * Exercise 15.01 walks through this file line by line -- and starts from a
 * broken copy of it.
 *
 * The part has 1 MB of flash at 0x08000000 and two SRAM banks: 96 KB of
 * SRAM1 at 0x20000000 and 32 KB of SRAM2 at 0x10000000. This script uses
 * SRAM1 for everything and leaves SRAM2 alone (RM0351 sec. 2.2.2; SRAM2 has
 * hardware parity and can survive standby -- 16.x mentions uses for it).
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 64K   /* TODO: check this against RM0351 sec. 2.2.2 */
  RAM2  (rwx) : ORIGIN = 0x10000000, LENGTH = 32K
}

/* The initial stack pointer: the top of SRAM1. The Cortex-M loads it from
 * the first vector-table entry before executing a single instruction. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Reserve space so the link FAILS if code + data leave less than this for
 * heap and stack. A firmware that links is claiming it fits. */
_min_heap_size = 0x2000;  /* newlib's dtoa (float printing) allocates */
_min_stack_size = 0x800;

SECTIONS
{
  /* The vector table must be the first thing in flash: the core fetches the
   * initial SP and PC from address 0, and BOOT0=0 maps flash there. KEEP()
   * defeats --gc-sections, which would otherwise discard a table nothing
   * ever calls. */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    . = ALIGN(4);
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* Unwind tables libgcc emits for a few of its routines. C code does not
   * unwind, but the linker still wants somewhere to put them. */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } >FLASH

  /* Addresses of functions to run before main(). The mect test harness
   * registers every TEST() through here -- 15.02 makes you walk this list
   * in your own startup code. */
  .init_array :
  {
    . = ALIGN(4);
    __init_array_start = .;
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array))
    __init_array_end = .;
    . = ALIGN(4);
  } >FLASH

  /* Initialised globals: they LIVE in RAM (VMA) but their initial values
   * must survive power-off, so the values are STORED in flash (LMA, the
   * "AT> FLASH"). Startup copies LENGTH bytes from _sidata to _sdata.
   * This is the .data trick every embedded target plays. */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* Zero-initialised globals: no flash copy needed, startup just zeroes the
   * range. This is why `static uint8_t buf[4096];` is free in flash and
   * `static uint8_t buf[4096] = {1};` is not. */
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } >RAM

  /* Heap grows up from `end` (newlib's _sbrk uses that symbol); the stack
   * grows down from _estack. If static data leaves less than the reserved
   * minimum between them, fail the link now rather than corrupt memory
   * later. */
  ._heap_stack_guard :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _min_heap_size;
    . = . + _min_stack_size;
    . = ALIGN(8);
  } >RAM
}
