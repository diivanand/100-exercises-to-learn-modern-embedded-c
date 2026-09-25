// Solution -- 12.01 Shared data: lost updates and torn reads

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- the code under test ------------------------------------------------------

static _Atomic uint32_t g_event_count;

void counter_increment(void) {
  // One indivisible read-modify-write. On a Cortex-M4 this compiles to an
  // LDREX/STREX retry loop; on the host, to a locked instruction. Either
  // way, no increment from the other stream can land in the middle of it.
  // relaxed is enough: only the TOTAL matters, nothing is published (12.05).
  atomic_fetch_add_explicit(&g_event_count, 1u, memory_order_relaxed);
}

uint32_t counter_read(void) {
  return atomic_load_explicit(&g_event_count, memory_order_relaxed);
}

// The sample and its integrity complement, packed into ONE 32-bit word so a
// single aligned store publishes both halves together. When shared data
// fits in one lock-free word, packing beats both volatile (which never
// provided atomicity) and a critical section (which costs interrupt
// latency). When it does not fit, see 12.03.
static _Atomic uint32_t g_sample = 0xFFFFu; // value 0, check ~0

void sample_publish(uint16_t value) {
  const uint32_t packed = ((uint32_t)value << 16) | (uint32_t)(uint16_t)~value;
  atomic_store_explicit(&g_sample, packed, memory_order_relaxed);
}

void sample_read(uint16_t *value, uint16_t *check) {
  const uint32_t packed = atomic_load_explicit(&g_sample, memory_order_relaxed);
  *value = (uint16_t)(packed >> 16);
  *check = (uint16_t)packed;
}

// --- the tests ------------------------------------------------------------------

enum { INCREMENTS_PER_STREAM = 200000 };

static _Atomic bool g_go;

static void *isr_counts_events(void *arg) {
  (void)arg;
  while (!atomic_load(&g_go)) {
    // line up with main so the two streams truly overlap
  }
  for (int i = 0; i < INCREMENTS_PER_STREAM; ++i) {
    counter_increment();
  }
  return NULL;
}

TEST("no increment is lost between the two streams") {
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, isr_counts_events, NULL) == 0);
  atomic_store(&g_go, true);
  for (int i = 0; i < INCREMENTS_PER_STREAM; ++i) {
    counter_increment();
  }
  pthread_join(isr, NULL);
  CHECK_EQ(counter_read(), 2u * INCREMENTS_PER_STREAM);
}

static _Atomic bool g_stop;

static void *isr_publishes_samples(void *arg) {
  (void)arg;
  uint16_t n = 0;
  while (!atomic_load(&g_stop)) {
    sample_publish(n);
    ++n;
  }
  return NULL;
}

TEST("a reader never sees half of one sample and half of another") {
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, isr_publishes_samples, NULL) == 0);

  uint32_t torn = 0;
  for (int i = 0; i < 400000; ++i) {
    uint16_t value = 0;
    uint16_t check = 0;
    sample_read(&value, &check);
    if (check != (uint16_t)~value) {
      ++torn;
    }
  }
  atomic_store(&g_stop, true);
  pthread_join(isr, NULL);
  CHECK_EQ(torn, 0u);
}
