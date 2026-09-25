// Solution -- 12.04 The SPSC ring: publish the data, THEN the index

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- the code under test ------------------------------------------------------

enum { RING_CAPACITY = 256 };
_Static_assert((RING_CAPACITY & (RING_CAPACITY - 1)) == 0,
               "capacity must be a power of two so & masks the index");

static uint8_t g_slots[RING_CAPACITY];
static _Atomic uint32_t g_head; // written by the producer only
static _Atomic uint32_t g_tail; // written by the consumer only

// Free-running indices (chapter 08's ring did the same): they wrap at
// 2^32, the difference is the fill level, and & masks them onto the array.

bool ring_push(uint8_t byte) {
  const uint32_t head = atomic_load_explicit(&g_head, memory_order_relaxed);
  // acquire pairs with the consumer's release below: by the time we see
  // its new tail, its READ of the slot we are about to overwrite is done.
  const uint32_t tail = atomic_load_explicit(&g_tail, memory_order_acquire);
  if (head - tail == (uint32_t)RING_CAPACITY) {
    return false; // full
  }
  g_slots[head & (RING_CAPACITY - 1u)] = byte;
  // release: the slot write above is visible BEFORE the new head. This
  // ordering is the entire correctness story of the lock-free ring.
  atomic_store_explicit(&g_head, head + 1u, memory_order_release);
  return true;
}

bool ring_pop(uint8_t *out) {
  const uint32_t tail = atomic_load_explicit(&g_tail, memory_order_relaxed);
  // acquire pairs with the producer's release: seeing the new head
  // guarantees the slot's bytes are there to read.
  const uint32_t head = atomic_load_explicit(&g_head, memory_order_acquire);
  if (head == tail) {
    return false; // empty
  }
  *out = g_slots[tail & (RING_CAPACITY - 1u)];
  atomic_store_explicit(&g_tail, tail + 1u, memory_order_release);
  return true;
}

// --- the tests ------------------------------------------------------------------

TEST("full means full, empty means empty") {
  for (uint32_t i = 0; i < (uint32_t)RING_CAPACITY; ++i) {
    CHECK(ring_push((uint8_t)i));
  }
  CHECK_FALSE(ring_push(0xAA)); // full: capacity is capacity
  uint8_t byte = 0;
  for (uint32_t i = 0; i < (uint32_t)RING_CAPACITY; ++i) {
    REQUIRE(ring_pop(&byte));
    CHECK_EQ(byte, (uint8_t)i);
  }
  CHECK_FALSE(ring_pop(&byte)); // empty again
}

enum { ITEM_COUNT = 400000 };

static uint8_t lcg_byte(uint32_t *state) {
  *state = *state * 1664525u + 1013904223u;
  return (uint8_t)(*state >> 24);
}

static void *producer(void *arg) {
  (void)arg;
  uint32_t rng = 1u;
  for (uint32_t i = 0; i < (uint32_t)ITEM_COUNT; ++i) {
    const uint8_t byte = lcg_byte(&rng);
    uint64_t patience = 0;
    while (!ring_push(byte)) {
      if (++patience > 200000000ull) {
        return NULL; // consumer is stuck; the count check will say so
      }
    }
  }
  return NULL;
}

TEST("every byte arrives, in order, uncorrupted") {
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, producer, NULL) == 0);

  uint32_t rng = 1u;
  uint32_t received = 0;
  uint32_t corrupt = 0;
  uint64_t patience = 0;
  while (received < (uint32_t)ITEM_COUNT) {
    uint8_t byte = 0;
    if (!ring_pop(&byte)) {
      if (++patience > 200000000ull) {
        break; // producer is stuck; the count check will say so
      }
      continue;
    }
    patience = 0;
    if (byte != lcg_byte(&rng)) {
      ++corrupt;
    }
    ++received;
  }
  pthread_join(isr, NULL);
  CHECK_EQ(received, (uint32_t)ITEM_COUNT);
  CHECK_EQ(corrupt, 0u);
}
