// Solution -- 15.03 SysTick: the heartbeat

#include <mect/mect.h>

#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

static volatile uint32_t g_ticks; // written by the handler, read by mainline

void SysTick_Handler(void) {
  ++g_ticks;
}

void systick_init(uint32_t tick_hz) {
  // RVR = cycles-per-tick MINUS ONE: the counter visits RVR, RVR-1, ..., 0,
  // which is RVR+1 states per lap. And CLKSOURCE_CPU, or everything runs at
  // an eighth of the speed you calculated.
  SYSTICK->RVR = BSP_SYSCLK_HZ / tick_hz - 1u;
  SYSTICK->CVR = 0; // any write clears; guarantees a full first lap
  SYSTICK->CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

uint32_t ticks_now(void) {
  return g_ticks;
}

void delay_ms(uint32_t ms) {
  // The 13.04 idiom: unsigned subtraction is modular, so this survives
  // g_ticks wrapping -- which a 1 kHz uint32_t tick does after 49.7 days,
  // longer than this test run but not longer than a product's life.
  const uint32_t start = ticks_now();
  while ((ticks_now() - start) < ms) {
  }
}

TEST("1 kHz configuration: reload, clock source, interrupt") {
  systick_init(1000);
  CHECK_EQ(SYSTICK->RVR, 3999u); // 4000 cycles per lap
  CHECK_EQ(SYSTICK->CSR & 0x7u,
           SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU);
}

TEST("the tick advances on its own") {
  const uint32_t start = ticks_now();
  uint32_t guard = 0;
  while ((ticks_now() - start) < 5u && ++guard < 2000000u) {
    // ~5 ms of real time; the guard turns a dead tick into a failure
    // instead of a silent hang.
  }
  CHECK((ticks_now() - start) >= 5u);
}

TEST("delay_ms waits what it says") {
  const uint32_t start = ticks_now();
  delay_ms(20);
  const uint32_t elapsed = ticks_now() - start;
  CHECK(elapsed >= 20u);
  CHECK(elapsed <= 22u); // generous: the harness prints between tests
}
