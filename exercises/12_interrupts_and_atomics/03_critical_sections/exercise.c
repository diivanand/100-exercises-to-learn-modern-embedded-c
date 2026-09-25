// =============================================================================
//  12.03 -- Critical sections: save, disable, RESTORE
// =============================================================================
//
//  When shared data does not fit in one lock-free word -- a three-field
//  context, a linked list, two counters that must move together -- atomics
//  alone cannot keep it coherent. The embedded answer is brutal and
//  effective: briefly make sure the other instruction stream CANNOT RUN.
//  On a Cortex-M4 that is one bit, PRIMASK, and the pattern is always the
//  same three lines (ARMv7-M B1.4.3):
//
//      uint32_t primask = __get_PRIMASK();   // SAVE the current state
//      __disable_irq();                      // CPSID I
//      ... touch the shared data ...
//      __set_PRIMASK(primask);               // RESTORE the saved state
//
//  Why restore, and never blindly enable? NESTING. A helper that protects
//  itself gets called from code that is already inside a section. If the
//  helper's exit ENABLES interrupts, it has re-opened the gate in the
//  middle of its caller's section -- the caller's "protected" data is now
//  fair game, and the bug appears only on the code path where the two
//  sections happen to nest. That is this exercise's starter, and it is a
//  genuinely common firmware defect.
//
//  The price of a critical section is INTERRUPT LATENCY: every cycle spent
//  masked is a cycle a UART byte or an encoder edge waits. Kept to a few
//  loads and stores it is nanoseconds; put a printf inside one and you have
//  designed a byte-loss generator. Keep sections short, keep them rare,
//  and prefer a single atomic word (12.01/12.02) whenever the data fits.
//
//  THE MODEL HERE. A mutex plays the interrupt gate: the "ISR" thread can
//  only fire while it can take the mutex, exactly as a real ISR can only
//  fire while PRIMASK is clear. The port layer is given; your job is the
//  two functions every firmware code base has, spelled here as
//  `cs_enter`/`cs_exit`.
//
//  TASK
//    Fix `cs_exit`: it must RESTORE the state its matching `cs_enter`
//    returned, not blindly unmask. Do not change the port model or the
//    tests.
//
//  RUN IT
//    ./mec test 12_03
//
// =============================================================================

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- the port model (given; do not edit) --------------------------------------

static pthread_mutex_t g_gate = PTHREAD_MUTEX_INITIALIZER;
static int g_masked;

static uint32_t port_mask_irqs(void) {
  const uint32_t was = (uint32_t)g_masked;
  if (!g_masked) {
    pthread_mutex_lock(&g_gate);
    g_masked = 1;
  }
  return was;
}

static void port_unmask_irqs(void) {
  if (g_masked) {
    g_masked = 0;
    pthread_mutex_unlock(&g_gate);
  }
}

static bool irqs_masked(void) {
  return g_masked != 0;
}

// --- the code under test ------------------------------------------------------

uint32_t cs_enter(void) {
  return port_mask_irqs();
}

void cs_exit(uint32_t token) {
  // TODO: this unmasks no matter what the matching cs_enter found. Inside
  // a nested use, the INNER exit re-opens the gate while the OUTER section
  // still believes it is protected.
  (void)token;
  port_unmask_irqs();
}

// --- the tests ------------------------------------------------------------------

TEST("nested sections stay masked until the OUTER exit") {
  const uint32_t outer = cs_enter();
  CHECK(irqs_masked());
  const uint32_t inner = cs_enter(); // a helper that also protects itself
  cs_exit(inner);
  CHECK(irqs_masked()); // still inside the outer section
  cs_exit(outer);
  CHECK_FALSE(irqs_masked());
}

// Shared state too wide for one atomic word: three fields the "ISR" always
// writes together, as (n, n, n).
static struct {
  uint32_t a, b, c;
} g_ctx;

static _Atomic bool g_isr_stop;

static void *isr_writes_ctx(void *arg) {
  (void)arg;
  uint32_t n = 1;
  while (!atomic_load(&g_isr_stop)) {
    pthread_mutex_lock(&g_gate); // an ISR only fires while unmasked
    g_ctx.a = n;
    g_ctx.b = n;
    g_ctx.c = n;
    pthread_mutex_unlock(&g_gate);
    ++n;
  }
  return NULL;
}

TEST("nothing fires inside a section, even around a nested one") {
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, isr_writes_ctx, NULL) == 0);

  uint32_t violations = 0;
  for (int round = 0; round < 2000; ++round) {
    const uint32_t outer = cs_enter();
    const uint32_t before = g_ctx.a;
    const uint32_t inner = cs_enter(); // e.g. a logging helper masks too
    cs_exit(inner);
    for (volatile int spin = 0; spin < 2000; ++spin) {
      // work happening "inside" the outer critical section
    }
    if (g_ctx.a != before) {
      ++violations;
    }
    cs_exit(outer);
  }
  atomic_store(&g_isr_stop, true);
  pthread_join(isr, NULL);
  CHECK_EQ(violations, 0u);
}
