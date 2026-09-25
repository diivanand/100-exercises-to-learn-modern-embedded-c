// Solution -- 15.06 Input: the blue button (INTERACTIVE)

#include <mect/mect.h>

#include "bsp.h"
#include "l476_regs.h"

#include <stdbool.h>
#include <stdint.h>

// --- given: tick + debouncer + prompt ------------------------------------------

static volatile uint32_t g_ticks;

void SysTick_Handler(void) {
  ++g_ticks;
}

static void systick_init_1khz(void) {
  SYSTICK->RVR = BSP_SYSCLK_HZ / 1000u - 1u;
  SYSTICK->CVR = 0;
  SYSTICK->CSR =
      SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

static void prompt(const char *s) {
  while (*s != '\0') {
    bsp_uart_putc(*s++);
  }
  bsp_uart_putc('\r');
  bsp_uart_putc('\n');
}

bool button_is_pressed(void);

static bool wait_debounced(bool want_pressed, uint32_t timeout_ms) {
  const uint32_t start = g_ticks;
  uint32_t agreeing = 0;
  uint32_t last_sample_tick = g_ticks;
  while ((g_ticks - start) < timeout_ms) {
    if (g_ticks != last_sample_tick) { // a new millisecond: take one sample
      last_sample_tick = g_ticks;
      agreeing = (button_is_pressed() == want_pressed) ? agreeing + 1 : 0;
      if (agreeing >= 8u) {
        return true;
      }
    }
  }
  return false;
}

// --- the driver -----------------------------------------------------------------

void button_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
  (void)RCC->AHB2ENR; // the enable must land before the port hears from us

  // Input mode is 00: only clear-then-nothing gets there from the analog
  // reset state. (No pull needed -- the Nucleo has one on the board; on a
  // bare pin you would add PUPDR pull-up here.)
  GPIOC->MODER &= ~(3u << (BSP_BUTTON_PIN * 2u));
}

bool button_is_pressed(void) {
  // IDR, not ODR -- the pin, not the latch. And inverted exactly once,
  // exactly here: the wiring is active-low; the API speaks human.
  return ((GPIOC->IDR >> BSP_BUTTON_PIN) & 1u) == 0u;
}

TEST("PC13 is a clocked input") {
  button_init();
  CHECK_EQ(RCC->AHB2ENR & RCC_AHB2ENR_GPIOCEN, RCC_AHB2ENR_GPIOCEN);
  CHECK_EQ((GPIOC->MODER >> (BSP_BUTTON_PIN * 2u)) & 3u, 0u);
}

TEST("idle reads released (hands off)") {
  systick_init_1khz();
  prompt("");
  prompt(">>> Do NOT touch the button for a moment...");
  CHECK(wait_debounced(false, 3000));
}

TEST("a press is seen, debounced (you have 20 seconds)") {
  prompt(">>> PRESS and HOLD the blue button (B1) now");
  CHECK(wait_debounced(true, 20000));
}

TEST("a release is seen, debounced") {
  prompt(">>> RELEASE it");
  CHECK(wait_debounced(false, 20000));
}
