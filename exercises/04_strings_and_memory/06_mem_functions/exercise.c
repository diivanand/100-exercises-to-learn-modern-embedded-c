// =============================================================================
//  04.06 -- The mem functions and their small print
// =============================================================================
//
//  memcpy, memmove, memset, memcmp: four calls that move most of the bytes
//  in most firmware. Each has one line of small print that bites
//  (Effective C ch. 7):
//
//  1. memcpy FORBIDS OVERLAP. Its prototype says so in the language
//     itself: both pointers are `restrict` (03.08) -- a promise the
//     regions are disjoint. Shifting a queue down over itself breaks that
//     promise, and it is undefined behaviour EVEN IF the values come out
//     right today, at this size, with this libc's choice of copy
//     direction. memmove is the one whose contract covers overlap: it
//     copies "as if through a temporary". Its cost over memcpy is a
//     comparison. When regions might overlap, that comparison was never
//     yours to save. (The asan build intercepts memcpy and reports
//     overlap outright -- run it on this starter.)
//
//  2. memset COUNTS BYTES, YOU COUNT ELEMENTS. memset(buf, 0, count)
//     zeroes count BYTES -- half of a uint16_t buffer, a quarter of a
//     uint32_t one. Spell sizes `count * sizeof *buf` and the expression
//     stays correct when the element type changes. (The related classic,
//     memset(p, 0, sizeof p) on a pointer, is caught by clang's
//     -Wsizeof-pointer-memaccess; the element/byte confusion is caught by
//     nobody but your tests.) Also mind the middle argument: it is
//     converted to unsigned char, so memset(buf, 0xFFFF, n) writes 0xFF.
//
//  3. memcmp COMPARES BYTES, NOT MEANING. Perfect for raw byte frames, as
//     below. Wrong for structs -- the padding between members holds
//     indeterminate bytes (05.01 measures them) -- and wrong for floats,
//     where -0.0 equals 0.0 but their bytes differ.
//
//  TASK
//    Fix `queue_compact` and `sample_reset`. `frame_repeats` is already
//    right -- read it and its comment for the contrast. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 04_06      (then: cmake --preset asan && ctest --preset asan)
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Drop the first `consumed` samples, moving the remainder to the front.
void queue_compact(uint16_t *samples, size_t count, size_t consumed) {
  // TODO: source and destination overlap; memcpy's contract forbids that.
  memcpy(samples, samples + consumed, (count - consumed) * sizeof *samples);
}

// Zero `count` samples.
void sample_reset(uint16_t *buf, size_t count) {
  // TODO: memset counts bytes, not uint16_t elements.
  memset(buf, 0, count);
}

// Are two received frames byte-for-byte identical?
bool frame_repeats(const uint8_t *a, const uint8_t *b, size_t n) {
  // memcmp is exactly right here: raw bytes, no padding, no semantics.
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
