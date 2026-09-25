// =============================================================================
//  12.01 -- Shared data: lost updates and torn reads
// =============================================================================
//
//  Firmware has two instruction streams on one core: the main loop, and the
//  interrupt handlers that preempt it between any two instructions. They
//  share memory, and everything in this chapter follows from one fact:
//
//      C STATEMENTS ARE NOT ATOMIC. `count++` is a load, an add and a
//      store, and an interrupt fits between any of them.
//
//  Two distinct disasters come out of that:
//
//  1. LOST UPDATES. Main loads count (say 41), the ISR fires and stores 42,
//     main resumes and stores its own 42. One event has vanished, silently,
//     with no fault raised. CERT CON43-C.
//
//  2. TORN READS. Data wider than one bus transfer -- a two-word timestamp,
//     a struct -- can be read half-old, half-new when the writer is
//     interrupted between the halves. The pair below carries a classic
//     integrity scheme from safety-standard code (IEC 60730 keeps RAM
//     copies with their complement): value plus ~value. A torn read is one
//     whose halves no longer match.
//
//  What `volatile` does about this: NOTHING. 11.01 established what
//  volatile is for (every access happens, in order, against the named
//  object); it never made an operation indivisible. The C11 answer is
//  <stdatomic.h>: `_Atomic uint32_t` with `atomic_fetch_add` is one
//  indivisible read-modify-write, and a 32-bit atomic store publishes all
//  32 bits together. On a Cortex-M4 those compile to LDREX/STREX loops --
//  lock-free, ISR-safe, no masking of interrupts required (12.02 digs in).
//  Data too wide for one atomic word needs a critical section: 12.03.
//
//  ON THE TEST BED. Your Mac plays the ISR with a POSIX thread, which is a
//  HARSHER environment than the real thing: two streams truly in parallel
//  on two cores, not interleaved on one. Code that survives here survives
//  there. The tests hammer enough times that the broken versions below fail
//  every run, not one run in fifty -- rerun them and see. ThreadSanitizer
//  makes the diagnosis precise:
//
//      cmake --preset tsan && ctest --preset tsan -R 12_01
//
//  TASK
//    Fix `counter_increment`/`counter_read` and `sample_publish`/
//    `sample_read` with <stdatomic.h>. The sample pair fits in one 32-bit
//    word -- pack it. Do not change the tests.
//
//  RUN IT
//    ./mec test 12_01
//
// =============================================================================

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- the code under test ------------------------------------------------------

static uint32_t g_event_count;

void counter_increment(void) {
  // TODO: a load, an add and a store; the other stream fits in between.
  g_event_count += 1u;
}

uint32_t counter_read(void) {
  return g_event_count;
}

// The published sample and its integrity complement (check == ~value).
static uint16_t g_value;
static uint16_t g_check = 0xFFFF;

void sample_publish(uint16_t value) {
  // TODO: two separate stores; a reader between them sees a mismatched pair.
  g_value = value;
  g_check = (uint16_t)~value;
}

void sample_read(uint16_t *value, uint16_t *check) {
  *value = g_value;
  *check = g_check;
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
