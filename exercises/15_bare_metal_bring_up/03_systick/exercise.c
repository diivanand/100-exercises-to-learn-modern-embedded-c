// =============================================================================
//  15.03 -- SysTick: the heartbeat
// =============================================================================
//
//  Every Cortex-M ships a 24-bit down-counter bolted to the core: SysTick
//  (ARMv7-M B3.3). Load it, start it, and it raises an exception each time
//  it wraps -- and that exception, counted in a variable, is the tick that
//  chapter 13's deadlines, timers and debouncers all stood on. Today the
//  fake tick becomes real.
//
//  Three registers (see bsp/l476_regs.h):
//
//    RVR  reload value. The counter counts RVR+1 cycles per lap: from RVR
//         down THROUGH zero. For a tick every N cycles, RVR = N - 1. The
//         off-by-one is not pedantry -- it is 0.025% of your clock, and
//         clocks drift into real money (a day has 86.4 million ms).
//    CVR  current value. WRITING ANY VALUE CLEARS IT (and COUNTFLAG) --
//         write it once before starting so the first tick is a full one.
//    CSR  control: ENABLE starts it, TICKINT routes the wrap to the
//         SysTick_Handler exception, CLKSOURCE_CPU counts CPU cycles.
//         Leave CLKSOURCE at 0 and you are counting HCLK/8 -- a factor of
//         eight your delays will inherit silently.
//
//  The clock is the reset default: MSI at 4 MHz (BSP_SYSCLK_HZ). So a 1 kHz
//  tick wants 4000 cycles per lap.
//
//  THE HANDLER. `SysTick_Handler` below overrides the weak default in
//  bsp/startup_stm32l476.c (the same mechanism as 15.02). It runs in
//  interrupt context: everything chapter 12 said about ISR/mainline sharing
//  applies, which is why the tick counter is volatile -- mainline code polls
//  it in a loop, exactly 11.01's situation. A uint32_t tick is naturally
//  atomic on this bus; a 64-bit one would not be (12.02).
//
//  DELAYS. `delay_ms` must survive tick wraparound, so it uses 13.04's
//  subtraction idiom -- (now - start) is modular arithmetic and works across
//  the wrap. Busy-waiting on a tick is fine HERE; 16.02 replaces it with a
//  scheduler, because a busy-wait starves everything else.
//
//  TASK
//    Fix systick_init (two bugs against the register descriptions above)
//    and implement delay_ms. The handler is given.
//
//  RUN IT
//    ./mec flash 15_03
//
// =============================================================================

#include <mect/mect.h>

#include "bsp.h"
#include "l476_regs.h"

#include <stdint.h>

static volatile uint32_t g_ticks; // written by the handler, read by mainline

void SysTick_Handler(void) {
  ++g_ticks;
}

void systick_init(uint32_t tick_hz) {
  // TODO: two of the three register writes below are wrong. Check each
  // against the header comment.
  SYSTICK->RVR = BSP_SYSCLK_HZ / tick_hz;
  SYSTICK->CVR = 0;
  SYSTICK->CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT;
}

uint32_t ticks_now(void) {
  return g_ticks;
}

void delay_ms(uint32_t ms) {
  // TODO: wait until `ms` ticks have elapsed, wraparound-safely (13.04).
  (void)ms;
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
