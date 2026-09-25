// =============================================================================
//  12.04 -- The SPSC ring: publish the data, THEN the index
// =============================================================================
//
//  The single-producer single-consumer ring buffer is THE data structure of
//  interrupt-driven I/O: the UART RX interrupt pushes bytes in, the main
//  loop pops them out, and neither ever has to wait for the other. Chapter
//  08 built the single-threaded ring; this exercise makes it survive two
//  instruction streams -- without a lock and without masking interrupts.
//
//  Why SPSC needs no lock: each index has exactly ONE writer. The producer
//  owns `head`, the consumer owns `tail`; each merely READS the other's
//  index. No read-modify-write is ever contended, so 12.01's lost-update
//  problem cannot occur. (The moment a second producer appears, that
//  argument collapses -- multi-producer queues are a different, harder
//  sport. Know which one you are building.)
//
//  What CAN go wrong is ORDERING. The producer does two writes per push:
//  the byte into the slot, and the new head. If the new head becomes
//  visible FIRST -- because the code stores it first, as the starter does,
//  or because the memory system reorders the stores, as ARM cores happily
//  do -- the consumer sees "data available", reads the slot, and gets
//  whatever was there a lap ago. The fix is a release/acquire pair:
//
//      slot = byte;                                   // 1: write the data
//      atomic_store_explicit(&head, h + 1, RELEASE);  // 2: then publish
//
//      h = atomic_load_explicit(&head, ACQUIRE);      // sees 2 -> sees 1
//      byte = slot;                                   // safe to read
//
//  Release says "everything I wrote before this store is visible to whoever
//  acquires it"; acquire says "everything the releaser wrote, I now see".
//  12.05 treats the memory orders in their own right.
//
//  The indices free-run and wrap at 2^32 (unsigned wraparound is defined --
//  02.03); `head - tail` is the fill level even across the wrap, and
//  `& (CAPACITY - 1)` masks them onto the array, which is why the capacity
//  must be a power of two.
//
//  THIS EXERCISE IS COMPILED -O2 (there is an `optimize` marker file in its
//  directory), and the starter shows you why that matters: with plain
//  uint32_t indices there is NO synchronisation in the program at all, so
//  the optimiser is entitled to keep `head` in a register across the
//  consumer's whole polling loop -- and does. The consumer never sees a
//  single byte ("received: 0"). Before the memory system can reorder your
//  stores, the compiler has already deleted your communication. Atomics fix
//  both, because both are the same problem: an unsynchronised program.
//  ThreadSanitizer names the bug precisely -- worth seeing:
//
//      cmake --preset tsan && ctest --preset tsan -R 12_04
//
//  TASK
//    Make `ring_push`/`ring_pop` safe: atomic indices, slot written before
//    head is published (release), head acquired before the slot is read.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 12_04
//
// =============================================================================

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
static uint32_t g_head; // written by the producer only
static uint32_t g_tail; // written by the consumer only

bool ring_push(uint8_t byte) {
  if (g_head - g_tail == (uint32_t)RING_CAPACITY) {
    return false; // full
  }
  // TODO: the index is published BEFORE the byte is written. A consumer
  // that wakes between these two lines reads a stale slot. (And plain
  // uint32_t indices give the compiler and the memory system licence to
  // surprise you even when the source order looks right.)
  g_head = g_head + 1u;
  g_slots[(g_head - 1u) & (RING_CAPACITY - 1u)] = byte;
  return true;
}

bool ring_pop(uint8_t *out) {
  if (g_head == g_tail) {
    return false; // empty
  }
  *out = g_slots[g_tail & (RING_CAPACITY - 1u)];
  g_tail = g_tail + 1u;
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
