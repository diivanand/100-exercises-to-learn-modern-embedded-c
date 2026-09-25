// =============================================================================
//  15.06 -- Input: the blue button (INTERACTIVE)
// =============================================================================
//
//  NOTE: this test needs YOU. It will ask you to press and release B1, the
//  blue button, and gives you twenty seconds each time. Hardware tests that
//  need a human are a last resort (Grenning: the target bottleneck), but an
//  input pin's final proof is a finger.
//
//  The electrical picture, because software cannot be right about a pin it
//  has misread: B1 connects PC13 to GROUND when pressed, and the Nucleo
//  routes it with an on-board pull-up (UM1724 sec. 6.5). So the pin idles
//  HIGH and reads LOW while pressed -- ACTIVE-LOW, like most buttons,
//  because pulling to ground is the cheap, noise-tolerant wiring. The
//  inversion belongs in ONE place: the driver you are writing. Let a raw
//  active-low level leak upward and every caller pays a `!` forever.
//
//  Two register facts:
//
//   - Port C, like most, resets MODER to all-analog (11). Input mode is 00:
//     another clear-the-field job -- |= cannot produce 00.
//   - IDR is the INPUT data register: the synchronised state of the pins.
//     ODR is the OUTPUT latch -- for a pin in input mode it is just a
//     register that remembers what you never wrote. Reading ODR to sense a
//     button compiles, runs, and reports a phantom world. (The starter
//     does exactly this.)
//
//  Debounce: the contact bounces for a few milliseconds (13.06 taught the
//  cure on fake ticks). Here `wait_debounced` demands 8 consecutive 1 ms
//  samples in agreement before believing anything.
//
//  TASK
//    Fix button_init and button_is_pressed. The debouncer and the tick are
//    given.
//
//  RUN IT
//    ./mec flash 15_06        (and keep a finger free)
//
// =============================================================================

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

// Waits (up to timeout_ms) until the button's DEBOUNCED state equals
// `want_pressed`: 8 consecutive 1 ms samples must agree.
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

// --- yours ----------------------------------------------------------------------

void button_init(void) {
  // TODO: the port is never clocked, and 11 cannot become 00 through |=.
  GPIOC->MODER |= (0u << (BSP_BUTTON_PIN * 2u));
}

bool button_is_pressed(void) {
  // TODO: wrong register, and no active-low inversion.
  return ((GPIOC->ODR >> BSP_BUTTON_PIN) & 1u) != 0u;
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
