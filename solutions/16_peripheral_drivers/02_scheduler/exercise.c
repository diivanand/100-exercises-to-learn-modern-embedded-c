// Solution -- 16.02 The superloop meets the real tick

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

// --- given: tick ---------------------------------------------------------------

static volatile uint32_t g_ticks;

void SysTick_Handler(void) {
  ++g_ticks;
}

static void systick_init_1khz(void) {
  SYSTICK->RVR = BSP_SYSCLK_HZ / 1000u - 1u;
  SYSTICK->CVR = 0;
  SYSTICK->CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

// --- the scheduler ---------------------------------------------------------------

struct task {
  void (*fn)(void);
  uint32_t period_ms;
  uint32_t next_due; // absolute tick; compared wraparound-safely (13.04)
};

void scheduler_run(struct task *tasks, size_t count, uint32_t run_ms) {
  const uint32_t t0 = g_ticks;
  for (size_t i = 0; i < count; ++i) {
    tasks[i].next_due = t0 + tasks[i].period_ms;
  }
  while ((g_ticks - t0) < run_ms) {
    const uint32_t now = g_ticks;
    for (size_t i = 0; i < count; ++i) {
      if ((int32_t)(now - tasks[i].next_due) >= 0) {
        tasks[i].fn();
        // Re-arm FROM THE DUE TIME, not from now. If something stalled the
        // loop, the backlog of missed slots stays due and the next passes
        // run them back to back -- the schedule catches up instead of
        // sliding. `next_due = now + period` forgets every slot the stall
        // ate, permanently; that is the starter's bug, and the 30 ms
        // blocker below makes it measurable.
        tasks[i].next_due += tasks[i].period_ms;
      }
    }
  }
}

// --- the workload -----------------------------------------------------------------

static uint32_t fast_runs, slow_runs;

static void fast_task(void) {
  ++fast_runs;
}

static void slow_task(void) {
  ++slow_runs;
}

// given: stands in for the occasional long job nobody admits to -- a flash
// page erase, a printf, a "quick" CRC over 64 KB. Runs once, mid-schedule.
static void blocker_task(void) {
  const uint32_t start = g_ticks;
  while ((g_ticks - start) < 30u) {
  }
}

TEST("a clean table holds both rates over one second") {
  systick_init_1khz();
  fast_runs = slow_runs = 0;
  struct task table[] = {
      {.fn = fast_task, .period_ms = 10},
      {.fn = slow_task, .period_ms = 100},
  };
  scheduler_run(table, 2, 1005);
  CHECK(fast_runs >= 99u && fast_runs <= 101u);
  CHECK(slow_runs >= 9u && slow_runs <= 11u);
}

TEST("a 30 ms stall costs no slots: the schedule catches up") {
  fast_runs = slow_runs = 0;
  struct task table[] = {
      {.fn = blocker_task, .period_ms = 700}, // fires once at t0 + 700
      {.fn = fast_task, .period_ms = 10},
      {.fn = slow_task, .period_ms = 100},
  };
  scheduler_run(table, 3, 1005);
  // The stall delays three fast slots (710, 720, 730); due-time re-arming
  // runs all three the moment the loop breathes again. Total stays ~100.
  // Re-arming from `now` would surrender them: ~97, outside the fence.
  CHECK(fast_runs >= 99u && fast_runs <= 102u);
  CHECK(slow_runs >= 8u && slow_runs <= 11u);
}
