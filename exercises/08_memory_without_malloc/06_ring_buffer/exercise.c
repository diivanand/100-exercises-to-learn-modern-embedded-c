// =============================================================================
//  08.06 -- The ring buffer: free-running indices, masked late
// =============================================================================
//
//  The third of the chapter's malloc replacements is the hardest working
//  data structure in firmware: the RING BUFFER, a fixed array that a
//  producer fills and a consumer drains, FIFO, forever. UART receive rings
//  (16.03), log rings, the ISR-to-mainline queue of 12.07 -- all this.
//
//  This implementation makes two decisions worth internalising:
//
//   1. CAPACITY IS A POWER OF TWO, so wrapping an index is one AND:
//      `index & (CAPACITY - 1)`. Not (only) because AND beats division --
//      there is a correctness reason. The indices below run freely and
//      wrap at 2^32; `x % CAPACITY` agrees with the masked value across
//      that wrap ONLY when CAPACITY divides 2^32, i.e. is a power of two.
//      A "% 10" ring is wrong once head wraps, four billion bytes later:
//      firmware lifetimes reach that. The _Static_assert (07.05) pins the
//      assumption to the constant it protects.
//
//   2. HEAD AND TAIL RUN FREELY -- they count every byte ever pushed and
//      popped, and are masked only at the moment of array access. The
//      occupancy is the plain difference `head - tail` (unsigned
//      subtraction stays right through the wrap, 13.04's idiom), so empty
//      is head == tail, full is head - tail == CAPACITY, EVERY slot is
//      usable, and no separate count variable exists to fall out of sync.
//      The popular alternative -- keep both indices pre-masked -- makes
//      full and empty indistinguishable (both are head == tail) and
//      forces you to waste a slot or carry a count. Know both; prefer
//      this one, not least because 12.04 can make it lock-free.
//
//  TASK
//    Two bugs, one per decision. ring_available masks BEFORE subtracting,
//    so it reports nonsense as soon as head and tail are in different
//    laps of the array. And ring_push's full check refuses at
//    CAPACITY - 1, wasting a slot this design does not need to waste.
//    Fix both. Do not change the tests.
//
//  RUN IT
//    ./mec test 08_06
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { RING_CAPACITY = 8 };

// The mask trick -- index & (CAPACITY-1) -- is only modulo arithmetic when
// CAPACITY is a power of two. Guard the assumption where it lives (07.05):
_Static_assert((RING_CAPACITY & (RING_CAPACITY - 1)) == 0,
               "the index mask requires a power-of-two capacity");

struct ring {
  uint8_t data[RING_CAPACITY];
  uint32_t head; // total bytes ever pushed
  uint32_t tail; // total bytes ever popped
};

size_t ring_available(const struct ring *r) {
  // TODO: masked too early. Subtract the free-running indices FIRST; mask
  // only when touching the array.
  return (size_t)((r->head & (RING_CAPACITY - 1u)) - (r->tail & (RING_CAPACITY - 1u)));
}

size_t ring_space(const struct ring *r) {
  return (size_t)RING_CAPACITY - ring_available(r);
}

bool ring_push(struct ring *r, uint8_t byte) {
  // TODO: this refuses one byte early -- a leftover habit from designs
  // where full and empty are ambiguous. In THIS design they are not.
  if (ring_available(r) >= RING_CAPACITY - 1u) {
    return false;
  }
  r->data[r->head & (RING_CAPACITY - 1u)] = byte;
  ++r->head; // data first, then publish -- 12.04 turns this order into law
  return true;
}

bool ring_pop(struct ring *r, uint8_t *out) {
  if (ring_available(r) == 0) {
    return false;
  }
  *out = r->data[r->tail & (RING_CAPACITY - 1u)];
  ++r->tail;
  return true;
}

TEST("fills to exactly its capacity, then refuses") {
  struct ring r = {0};
  for (uint32_t i = 0; i < RING_CAPACITY; ++i) {
    CHECK(ring_push(&r, (uint8_t)(0x10u + i)));
  }
  CHECK_EQ(ring_available(&r), (size_t)RING_CAPACITY); // all 8 slots hold data
  CHECK_EQ(ring_space(&r), 0u);
  CHECK_FALSE(ring_push(&r, 0xFF)); // the ninth byte has nowhere to live
}

TEST("drains in arrival order, then reports empty") {
  struct ring r = {0};
  for (uint32_t i = 0; i < RING_CAPACITY; ++i) {
    CHECK(ring_push(&r, (uint8_t)(0x10u + i)));
  }
  for (uint32_t i = 0; i < RING_CAPACITY; ++i) {
    uint8_t byte = 0;
    CHECK(ring_pop(&r, &byte));
    CHECK_EQ(byte, (uint8_t)(0x10u + i)); // FIFO: first in, first out
  }
  uint8_t byte = 0;
  CHECK_FALSE(ring_pop(&r, &byte));
  CHECK_EQ(ring_available(&r), 0u);
}

TEST("keeps working as the indices wrap past the array edge") {
  struct ring r = {0};
  // Keep five bytes in flight while pushing far more than CAPACITY bytes
  // through: every index wraps several times, and the masked-too-early
  // bug (mask THEN subtract) falls over the first time head and tail land
  // in different laps of the array.
  for (uint32_t i = 0; i < 5; ++i) {
    CHECK(ring_push(&r, (uint8_t)i));
  }
  for (uint32_t i = 0; i < 40; ++i) {
    CHECK(ring_push(&r, (uint8_t)(i + 5u)));
    uint8_t byte = 0;
    CHECK(ring_pop(&r, &byte));
    CHECK_EQ(byte, (uint8_t)i);
    CHECK_EQ(ring_available(&r), 5u);
  }
}
