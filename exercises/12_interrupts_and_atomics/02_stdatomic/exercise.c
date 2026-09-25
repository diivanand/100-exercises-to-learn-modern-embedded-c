// =============================================================================
//  12.02 -- stdatomic.h: fetch-and-op, exchange, compare-exchange
// =============================================================================
//
//  12.01 used `atomic_fetch_add`. This exercise fills out the toolbox,
//  because almost every ISR/mainline design reduces to three shapes:
//
//   - FETCH-AND-OP: `atomic_fetch_or(&pending, EVENT_RX)` posts an event
//     bit; `atomic_fetch_add` counts. Indivisible read-modify-writes.
//
//   - EXCHANGE: `atomic_exchange(&pending, 0)` takes the whole mask AND
//     clears it in one step. Compare with the broken drain below: load,
//     then store zero -- an event posted between the two is wiped without
//     ever being seen. TOCTOU with a two-instruction window.
//
//   - COMPARE-EXCHANGE: the retry loop for "read, decide, write back only
//     if nobody interfered". The token pool below is the canonical use:
//     check there is stock AND take one, indivisibly. `_weak` may fail
//     spuriously and belongs in a loop (CERT CON41-C); it is the cheap one
//     on LDREX/STREX machines like the Cortex-M4.
//
//  WHAT IS LOCK-FREE, ON YOUR TARGET. The macros ATOMIC_*_LOCK_FREE answer
//  at compile time; the _Static_assert below pins the 32-bit case for this
//  course. On a Cortex-M4, up-to-32-bit atomics are lock-free (LDREX/
//  STREX); a 64-bit `_Atomic` is NOT -- the compiler quietly calls a
//  library routine that masks interrupts. Legal, but the latency you were
//  avoiding is back. Keep ISR-shared atomics at word size.
//
//  The event-flag word here plus the ring of 12.04 make up the deferred-
//  work pattern that 12.07 assembles -- the shape of most real firmware.
//
//  TASK
//    Fix `events_drain` (exchange), `events_post` (fetch-or), and
//    `token_take` (a compare-exchange loop). Do not change the tests. The
//    drain test detects a wiped event as "no progress": the poster stops
//    being acknowledged and the test gives up and fails.
//
//  RUN IT
//    ./mec test 12_02
//
// =============================================================================

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// Both targets this course cares about have lock-free 32-bit atomics; if
// this ever fails, every "no masking needed" claim above is off the table.
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "32-bit atomics must be lock-free");

// --- the code under test ------------------------------------------------------

enum {
  EVENT_RX = 1u << 0,
  EVENT_TX = 1u << 1,
  EVENT_TICK = 1u << 2,
  EVENT_ERROR = 1u << 3,
};

static uint32_t g_pending;

void events_post(uint32_t mask) {
  // TODO: |= is a load and a store; a post from the other stream between
  // them is lost (12.01 again -- it never stops applying).
  g_pending |= mask;
}

uint32_t events_drain(void) {
  // TODO: an event posted between these two lines is wiped, never seen.
  const uint32_t taken = g_pending;
  g_pending = 0u;
  return taken;
}

static uint32_t g_tokens;

void tokens_reset(uint32_t n) {
  g_tokens = n;
}

uint32_t tokens_left(void) {
  return g_tokens;
}

bool token_take(void) {
  // TODO: check-then-take with a gap in the middle. Two streams both read
  // the same count, both write back count - 1: one decrement vanishes and
  // the pool has double-spent a token.
  const uint32_t n = g_tokens;
  if (n > 0u) {
    g_tokens = n - 1u;
    return true;
  }
  return false;
}

// --- the tests ------------------------------------------------------------------

enum { HANDSHAKES = 2000 };

static _Atomic uint32_t g_acked;
static _Atomic bool g_abort;

static void *isr_posts_rx(void *arg) {
  (void)arg;
  for (uint32_t i = 0; i < HANDSHAKES; ++i) {
    events_post(EVENT_RX);
    while (atomic_load(&g_acked) <= i) { // wait for main to see this one
      if (atomic_load(&g_abort)) {
        return NULL;
      }
    }
  }
  return NULL;
}

TEST("an event posted mid-drain is never wiped unseen") {
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, isr_posts_rx, NULL) == 0);

  uint32_t seen = 0;
  uint64_t quiet_polls = 0;
  while (seen < HANDSHAKES) {
    if (events_drain() & EVENT_RX) {
      ++seen;
      atomic_store(&g_acked, seen);
      quiet_polls = 0;
    } else if (++quiet_polls > 20000000ull) {
      break; // the pending event was wiped; no progress is coming
    }
  }
  atomic_store(&g_abort, true);
  pthread_join(isr, NULL);
  CHECK_EQ(seen, (uint32_t)HANDSHAKES);
}

enum { TOKEN_COUNT = 100000 };

static _Atomic uint32_t g_isr_takes;

static void *isr_takes_tokens(void *arg) {
  (void)arg;
  uint32_t mine = 0;
  while (token_take()) {
    ++mine;
  }
  atomic_store(&g_isr_takes, mine);
  return NULL;
}

TEST("a token pool never double-spends") {
  tokens_reset(TOKEN_COUNT);
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, isr_takes_tokens, NULL) == 0);
  uint32_t mine = 0;
  while (token_take()) {
    ++mine;
  }
  pthread_join(isr, NULL);
  CHECK_EQ(mine + atomic_load(&g_isr_takes), (uint32_t)TOKEN_COUNT);
  CHECK_EQ(tokens_left(), 0u);
}
