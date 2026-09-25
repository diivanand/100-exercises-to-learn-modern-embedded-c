// Startup code for the STM32L476RG: what happens between power-on and main().
//
// There is no loader and no crt0 here (-nostartfiles); this file is
// everything. The hardware's contribution is exactly two reads: the core
// loads its stack pointer from flash offset 0 and its program counter from
// offset 4. From that second read onwards -- Reset_Handler -- it is all C.
//
// Exercise 15.02 asks you to reimplement the three hooks below; this file's
// versions are `weak`, so a strong definition in an exercise wins at link
// time. (The weak-symbol mechanism is a toolchain feature, not ISO C -- and
// it is exactly how every vendor's startup file lets you replace a default
// interrupt handler with your own. See 10.x for the seam pattern in general.)

#include <stdint.h>

#include "l476_regs.h"

// Symbols the linker script defines. Their ADDRESSES are the values; the
// `uint32_t` type is a polite fiction (15.01 explains).
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

typedef void (*init_fn)(void);
extern init_fn __init_array_start[], __init_array_end[];

int main(void);
void bsp_init(void);

// --- the three jobs before main (weak: exercises override them) --------------

// Job 1: initialised globals live in RAM but their values are in flash;
// copy them over. Until this runs, every initialised global reads garbage.
__attribute__((weak)) void startup_copy_data(void) {
  const uint32_t *src = &_sidata;
  for (uint32_t *dst = &_sdata; dst < &_edata; ++dst) {
    *dst = *src++;
  }
}

// Job 2: the C standard promises zero-initialised statics; nobody zeroes an
// SRAM cell but us.
__attribute__((weak)) void startup_zero_bss(void) {
  for (uint32_t *dst = &_sbss; dst < &_ebss; ++dst) {
    *dst = 0;
  }
}

// Job 3: run the "constructors" -- functions the compiler collected into
// .init_array. The mect harness registers every TEST() through one of
// these, so if this walk is skipped, the run reports "no tests were
// registered". That is the failure mode 15.02 starts in.
__attribute__((weak)) void startup_call_init_array(void) {
  for (init_fn *fn = __init_array_start; fn < __init_array_end; ++fn) {
    (*fn)();
  }
}

// --- reset --------------------------------------------------------------------

void Reset_Handler(void) {
  // Let the compiler use the FPU before anything float happens: grant full
  // access to coprocessors 10 and 11 (ARMv7-M B3.2.20). With
  // -mfloat-abi=hard, touching a float before this line is a UsageFault.
  SCB_CPACR |= (0xFu << 20);

  // Point the vector table at ourselves explicitly. BOOT0=0 already aliases
  // flash to address 0, but being explicit survives a bootloader.
  SCB->VTOR = 0x08000000u;

  startup_copy_data();
  startup_zero_bss();
  startup_call_init_array();

  bsp_init(); // clocks + the UART the harness prints through
  (void)main();

  // main returned: on a desktop the OS cleans up; here there is nowhere to
  // go. Sleep forever, waking only for interrupts.
  for (;;) {
    wait_for_interrupt();
  }
}

// --- default handlers -----------------------------------------------------------

// A fault or an unexpected interrupt lands here and spins, which at least
// keeps the crime scene intact for a debugger. Every handler is a weak alias
// of this; define a function with the right name (16.01 does) and yours wins.
void Default_Handler(void) {
  for (;;) {
  }
}

#define WEAK_DEFAULT __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void) WEAK_DEFAULT;
void HardFault_Handler(void) WEAK_DEFAULT;
void MemManage_Handler(void) WEAK_DEFAULT;
void BusFault_Handler(void) WEAK_DEFAULT;
void UsageFault_Handler(void) WEAK_DEFAULT;
void SVC_Handler(void) WEAK_DEFAULT;
void DebugMon_Handler(void) WEAK_DEFAULT;
void PendSV_Handler(void) WEAK_DEFAULT;
void SysTick_Handler(void) WEAK_DEFAULT;

void EXTI0_IRQHandler(void) WEAK_DEFAULT;
void EXTI1_IRQHandler(void) WEAK_DEFAULT;
void EXTI2_IRQHandler(void) WEAK_DEFAULT;
void EXTI3_IRQHandler(void) WEAK_DEFAULT;
void EXTI4_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH1_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH2_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH3_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH4_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH5_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH6_IRQHandler(void) WEAK_DEFAULT;
void DMA1_CH7_IRQHandler(void) WEAK_DEFAULT;
void ADC1_2_IRQHandler(void) WEAK_DEFAULT;
void EXTI9_5_IRQHandler(void) WEAK_DEFAULT;
void TIM2_IRQHandler(void) WEAK_DEFAULT;
void SPI1_IRQHandler(void) WEAK_DEFAULT;
void SPI2_IRQHandler(void) WEAK_DEFAULT;
void USART1_IRQHandler(void) WEAK_DEFAULT;
void USART2_IRQHandler(void) WEAK_DEFAULT;
void USART3_IRQHandler(void) WEAK_DEFAULT;
void EXTI15_10_IRQHandler(void) WEAK_DEFAULT;

// --- the vector table -----------------------------------------------------------
//
// Entry 0 is not code but the initial stack pointer, so the table cannot be
// a plain array of function pointers -- ISO C forbids converting an object
// pointer to a function pointer. A union of the two pointer kinds holds
// either without a cast. KEEP() in the linker script pins the table down;
// `used` stops the compiler from discarding what nothing references.

union vector {
  void (*handler)(void);
  const void *sp;
};

__attribute__((section(".isr_vector"),
               used)) static const union vector vector_table[16 + 82] = {
    [0] = {.sp = &_estack},
    [1] = {.handler = Reset_Handler},
    [2] = {.handler = NMI_Handler},
    [3] = {.handler = HardFault_Handler},
    [4] = {.handler = MemManage_Handler},
    [5] = {.handler = BusFault_Handler},
    [6] = {.handler = UsageFault_Handler},
    // 7..10 reserved
    [11] = {.handler = SVC_Handler},
    [12] = {.handler = DebugMon_Handler},
    // 13 reserved
    [14] = {.handler = PendSV_Handler},
    [15] = {.handler = SysTick_Handler},
    // External interrupts: table position = 16 + IRQ number (RM0351
    // table 58). Only the vectors the course uses are wired up; the
    // designated initialiser leaves every other slot NULL, and the
    // course never enables those IRQs. (A production table wires ALL
    // of them to Default_Handler -- an enabled-by-accident interrupt
    // through a NULL vector is a fault with the evidence missing.)
    [16 + 6] = {.handler = EXTI0_IRQHandler},
    [16 + 7] = {.handler = EXTI1_IRQHandler},
    [16 + 8] = {.handler = EXTI2_IRQHandler},
    [16 + 9] = {.handler = EXTI3_IRQHandler},
    [16 + 10] = {.handler = EXTI4_IRQHandler},
    [16 + 11] = {.handler = DMA1_CH1_IRQHandler},
    [16 + 12] = {.handler = DMA1_CH2_IRQHandler},
    [16 + 13] = {.handler = DMA1_CH3_IRQHandler},
    [16 + 14] = {.handler = DMA1_CH4_IRQHandler},
    [16 + 15] = {.handler = DMA1_CH5_IRQHandler},
    [16 + 16] = {.handler = DMA1_CH6_IRQHandler},
    [16 + 17] = {.handler = DMA1_CH7_IRQHandler},
    [16 + 18] = {.handler = ADC1_2_IRQHandler},
    [16 + 23] = {.handler = EXTI9_5_IRQHandler},
    [16 + 28] = {.handler = TIM2_IRQHandler},
    [16 + 35] = {.handler = SPI1_IRQHandler},
    [16 + 36] = {.handler = SPI2_IRQHandler},
    [16 + 37] = {.handler = USART1_IRQHandler},
    [16 + 38] = {.handler = USART2_IRQHandler},
    [16 + 39] = {.handler = USART3_IRQHandler},
    [16 + 40] = {.handler = EXTI15_10_IRQHandler},
};
