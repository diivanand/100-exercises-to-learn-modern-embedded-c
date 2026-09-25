// Solution -- 16.01 EXTI: the button becomes an interrupt (INTERACTIVE)

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

// --- the driver -----------------------------------------------------------------

static volatile uint32_t g_edges; // falling edges seen by the handler

void EXTI15_10_IRQHandler(void) {
  if (EXTI->PR1 & (1u << BSP_BUTTON_PIN)) {
    // Write-1-to-clear, with `=`: exactly this line is what 11.04 was for.
    // `|=` would read every pending line in PR1 and write them all back as
    // ones -- acknowledging interrupts this handler never looked at.
    EXTI->PR1 = 1u << BSP_BUTTON_PIN;
    ++g_edges;
  }
}

void button_irq_init(void) {
  // The pin itself, as in 15.06: clocked port, input mode.
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
  (void)RCC->AHB2ENR;
  GPIOC->MODER &= ~(3u << (BSP_BUTTON_PIN * 2u));

  // Route the LINE. EXTI line 13 can watch pin 13 of any port; SYSCFG's
  // EXTICR decides which. Four lines per register, a nibble each:
  // line 13 -> EXTICR[13 / 4] = EXTICR[3], nibble 13 % 4 = 1, port C = 2
  // (RM0351 9.2.6). The reset value routes every line to port A.
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
  (void)RCC->APB2ENR;
  SYSCFG->EXTICR[3] &= ~(0xFu << 4);
  SYSCFG->EXTICR[3] |= 2u << 4;

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
