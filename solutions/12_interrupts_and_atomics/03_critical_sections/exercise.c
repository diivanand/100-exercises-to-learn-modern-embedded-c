// Solution -- 12.03 Critical sections: save, disable, RESTORE

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- the port model (given; do not edit) --------------------------------------
//
// On the Cortex-M4 the real implementation is three instructions around
// PRIMASK (ARMv7-M B1.4.3):
//
//     uint32_t primask = __get_PRIMASK();   // save
//     __disable_irq();                      // CPSID I
//     ...
//     __set_PRIMASK(primask);               // restore -- NOT blindly enable
//
// On the host, a mutex plays the interrupt gate: the "ISR" thread can only
// fire while it can take the mutex, exactly as a real ISR can only fire
// while PRIMASK is clear. g_masked mirrors the gate for the tests to
// inspect; only the main thread touches it.

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
  // Restore what the matching cs_enter SAW. Only the outermost exit -- the
  // one whose enter found interrupts unmasked -- actually unmasks. A blind
  // unmask here re-opens the gate inside any enclosing section, which is
  // the classic nesting bug this exercise starts with.
  if (token == 0u) {
    port_unmask_irqs();
  }
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
