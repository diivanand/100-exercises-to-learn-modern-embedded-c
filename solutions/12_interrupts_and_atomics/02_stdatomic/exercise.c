// Solution -- 12.02 stdatomic.h: fetch-and-op, exchange, compare-exchange

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// Both targets this course cares about have lock-free 32-bit atomics; if
// this ever fails, every "no masking needed" claim below is off the table.
_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "32-bit atomics must be lock-free");

// --- the code under test ------------------------------------------------------

enum {
  EVENT_RX = 1u << 0,
  EVENT_TX = 1u << 1,
  EVENT_TICK = 1u << 2,
  EVENT_ERROR = 1u << 3,
};

static _Atomic uint32_t g_pending;

void events_post(uint32_t mask) {
  // One indivisible OR: posts from the other stream cannot vanish under it.
  atomic_fetch_or(&g_pending, mask);
}

uint32_t events_drain(void) {
  // Exchange takes everything and leaves zero in ONE step. The gap between
  // "read the mask" and "clear the mask" -- where the starter lost events
  // -- does not exist any more.
  return atomic_exchange(&g_pending, 0u);
}

static _Atomic uint32_t g_tokens;

void tokens_reset(uint32_t n) {
  atomic_store(&g_tokens, n);
}

uint32_t tokens_left(void) {
  return atomic_load(&g_tokens);
}

bool token_take(void) {
  // The compare-exchange retry loop: read, compute, attempt to swap in the
  // result IF nothing changed meanwhile; on failure `n` is refreshed with
  // the current value and we decide again. `weak` may fail spuriously,
  // which is fine inside a loop (CERT CON41-C) and cheaper on LDREX/STREX
  // machines -- which a Cortex-M4 is.
  uint32_t n = atomic_load(&g_tokens);
  while (n > 0u) {
    if (atomic_compare_exchange_weak(&g_tokens, &n, n - 1u)) {
      return true;
    }
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
