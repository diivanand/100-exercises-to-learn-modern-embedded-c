// =============================================================================
//  16.01 -- EXTI: the button becomes an interrupt (INTERACTIVE)
// =============================================================================
//
//  15.06 polled PC13 a thousand times a second and found it idle almost
//  every time. The interrupt way inverts the deal: the hardware watches the
//  pin, and your code runs only when something happened. The plumbing has
//  three stations, and a press must clear all three:
//
//    pin PC13 --SYSCFG--> EXTI line 13 --EXTI--> NVIC --> your handler
//
//  1. ROUTING. EXTI line 13 can watch pin 13 of ANY port -- one line per
//     pin number, shared across ports. SYSCFG->EXTICR chooses the port:
//     four lines per register, one nibble each, so line 13 lives in
//     EXTICR[13 / 4] at nibble 13 % 4, and port C is code 2 (RM0351
//     9.2.6). THE RESET VALUE ROUTES EVERY LINE TO PORT A -- a line you
//     never route works exactly never, with no error anywhere.
//
//  2. THE LINE. FTSR1 selects the falling edge (a press pulls low, 15.06),
//     IMR1 unmasks the line. SWIER1 lets software fake the edge -- the
//     tests use it, and so should you: it is the difference between "the
//     config is wrong" and "my finger missed the window".
//
//  3. THE NVIC. Line 13 arrives on IRQ 40 (EXTI15_10 -- lines 10..15
//     share it; a real handler for several lines reads PR1 to see which).
//     nvic_enable_irq(40) sets the bit; the handler name must be
//     EXTI15_10_IRQHandler, because that is the symbol the vector table
//     (bsp/startup_stm32l476.c) wired to slot 16 + 40.
//
//  THE HANDLER'S DUTY: acknowledge. PR1 is write-1-to-clear (11.04): write
//  `1u << 13` with `=`. Forget it and the interrupt refires forever the
//  moment it returns; use `|=` and you acknowledge every pending line you
//  read, handled or not. The starter's handler has the `|=` spelling --
//  with only one line in use it happens to work, which is exactly why the
//  habit survives code review after code review. Fix it on principle.
//
//  NOTE: the observable starter failures are the ROUTING (test 1, then no
//  real press can arrive in test 3) -- the `|=` cannot be caught by a test
//  while line 13 is the only line alive.
//
//  TASK
//    Complete button_irq_init (the routing block is missing) and correct
//    the handler's acknowledge.
//
//  RUN IT
//    ./mec flash 16_01        (finger ready)
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

// --- given: tick + prompt ------------------------------------------------------

static volatile uint32_t g_ticks;

void SysTick_Handler(void) {
  ++g_ticks;
}

static void systick_init_1khz(void) {
  SYSTICK->RVR = BSP_SYSCLK_HZ / 1000u - 1u;
  SYSTICK->CVR = 0;
  SYSTICK->CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

static void prompt(const char *s) {
  while (*s != '\0') {
    bsp_uart_putc(*s++);
  }
  bsp_uart_putc('\r');
  bsp_uart_putc('\n');
}

// --- yours ----------------------------------------------------------------------

static volatile uint32_t g_edges; // falling edges seen by the handler

void EXTI15_10_IRQHandler(void) {
  if (EXTI->PR1 & (1u << BSP_BUTTON_PIN)) {
    // TODO: this is the |= acknowledge the header warned about.
    EXTI->PR1 |= 1u << BSP_BUTTON_PIN;
    ++g_edges;
  }
}

void button_irq_init(void) {
  // The pin itself, as in 15.06: clocked port, input mode.
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
  (void)RCC->AHB2ENR;
  GPIOC->MODER &= ~(3u << (BSP_BUTTON_PIN * 2u));

  // TODO: route EXTI line 13 to port C (SYSCFG clock + EXTICR nibble).
  // Until then the line watches PA13 -- the SWD clock line, not your button.

  // Falling edge (a press pulls the line low), unmask, and let the NVIC
  // deliver it.
  EXTI->FTSR1 |= 1u << BSP_BUTTON_PIN;
  EXTI->IMR1 |= 1u << BSP_BUTTON_PIN;
  nvic_enable_irq(IRQN_EXTI15_10);
}

TEST("the line is routed, edged, unmasked and NVIC-enabled") {
  button_irq_init();
  CHECK_EQ((SYSCFG->EXTICR[3] >> 4) & 0xFu, 2u); // port C, not reset port A
  CHECK_EQ(EXTI->FTSR1 & (1u << BSP_BUTTON_PIN), 1u << BSP_BUTTON_PIN);
  CHECK_EQ(EXTI->IMR1 & (1u << BSP_BUTTON_PIN), 1u << BSP_BUTTON_PIN);
  // IRQ 40 lives in ISER[1], bit 40 - 32 = 8.
  CHECK_EQ(NVIC->ISER[1] & (1u << 8), 1u << 8);
}

TEST("a software trigger reaches the handler (no finger involved)") {
  const uint32_t before = g_edges;
  EXTI->SWIER1 = 1u << BSP_BUTTON_PIN; // sets PR1 as if the edge happened
  for (volatile int i = 0; i < 100; ++i) {
    // a few cycles for the exception to be taken
  }
  CHECK_EQ(g_edges, before + 1u);
  CHECK_EQ(EXTI->PR1 & (1u << BSP_BUTTON_PIN), 0u); // acknowledged, once
}

TEST("a real press arrives as an interrupt (you have 20 seconds)") {
  systick_init_1khz();
  prompt("");
  prompt(">>> PRESS the blue button (B1)");
  const uint32_t before = g_edges;
  const uint32_t start = g_ticks;
  while (g_edges == before && (g_ticks - start) < 20000u) {
  }
  // At least one edge. Very likely MORE than one: the contact bounces, and
  // EXTI counts edges, not intentions. Turning edges into presses is 13.06's
  // debounce job -- on top of the interrupt, not instead of it.
  CHECK(g_edges > before);
}
