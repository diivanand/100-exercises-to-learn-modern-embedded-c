// Solution -- 04.06 The mem functions and their small print

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void queue_compact(uint16_t *samples, size_t count, size_t consumed) {
  // The regions overlap whenever fewer than half the samples were
  // consumed, and overlapping copies are memmove's job -- memcpy's
  // contract (its restrict-qualified prototype, 03.08) forbids them.
  // Precondition: consumed <= count; the caller owns that (09.06).
  memmove(samples, samples + consumed, (count - consumed) * sizeof *samples);
}

void sample_reset(uint16_t *buf, size_t count) {
  // memset counts BYTES. `count * sizeof *buf` says so, and survives the
  // day somebody widens the samples to uint32_t.
  memset(buf, 0, count * sizeof *buf);
}

bool frame_repeats(const uint8_t *a, const uint8_t *b, size_t n) {
  // memcmp is exactly right here: raw bytes, no padding, no semantics.
  // It is exactly WRONG for structs (indeterminate padding bytes, 05.01)
  // and for floats (-0.0 == 0.0 but their bytes differ).
  return memcmp(a, b, n) == 0;
}

TEST("compacting a queue moves the tail to the front") {
  uint16_t q[8] = {10, 11, 12, 13, 14, 15, 16, 17};
  queue_compact(q, 8, 2);
  const uint16_t expect[6] = {12, 13, 14, 15, 16, 17};
  CHECK_MEM_EQ(q, expect, sizeof expect);
}

TEST("compacting by one is the worst overlap, and must still work") {
  uint16_t q[4] = {100, 200, 300, 400};
  queue_compact(q, 4, 1);
  const uint16_t expect[3] = {200, 300, 400};
  CHECK_MEM_EQ(q, expect, sizeof expect);
}

TEST("resetting clears every sample, not every byte") {
  uint16_t buf[8];
  for (size_t i = 0; i < 8; ++i) {
    buf[i] = 0xFFFF;
  }
  sample_reset(buf, 8);
  for (size_t i = 0; i < 8; ++i) {
    CHECK_EQ(buf[i], 0u);
  }
}

TEST("byte buffers compare with memcmp") {
  const uint8_t a[] = {1, 2, 3, 4};
  const uint8_t b[] = {1, 2, 3, 4};
  const uint8_t c[] = {1, 2, 9, 4};
  CHECK(frame_repeats(a, b, sizeof a));
  CHECK_FALSE(frame_repeats(a, c, sizeof a));
}
