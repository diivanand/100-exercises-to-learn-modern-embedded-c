// Solution -- 08.06 The ring buffer: free-running indices, masked late

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

// head and tail RUN FREELY and are masked only at the array access. The
// difference head - tail is the occupancy, and unsigned subtraction keeps
// it correct straight through the 2^32 wrap (13.04 makes a habit of this).
// Full and empty stop being ambiguous: empty is head == tail, full is
// head - tail == CAPACITY, and every slot is usable.
size_t ring_available(const struct ring *r) {
  return (size_t)(r->head - r->tail);
}

size_t ring_space(const struct ring *r) {
  return (size_t)RING_CAPACITY - ring_available(r);
}

bool ring_push(struct ring *r, uint8_t byte) {
  if (ring_available(r) == RING_CAPACITY) {
    return false; // full: the caller decides what dropping means
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
