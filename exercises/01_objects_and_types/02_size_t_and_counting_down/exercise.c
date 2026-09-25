// =============================================================================
//  01.02 -- size_t, and how to count down with it
// =============================================================================
//
//  Sizes, lengths and indices are size_t: the unsigned type sizeof yields,
//  guaranteed to hold the size of any object. Pointer differences are
//  ptrdiff_t, its signed sibling. Using int for either works until the day
//  an object outgrows it -- and using size_t brings one sharp edge you must
//  learn to hold, because it is unsigned. (Effective C ch. 3; CERT INT01-C:
//  use size_t for sizes.)
//
//  THE EDGE: an unsigned type cannot go below zero -- it WRAPS. That is
//  defined behaviour (02.03 compares it with signed overflow, which is not),
//  but defined does not mean harmless. The classic casualty is the backwards
//  loop:
//
//      for (size_t i = len - 1; i >= 0; --i)     // never terminates
//
//  `i >= 0` is true for every unsigned value there is. When i reaches 0 and
//  decrements, it wraps to SIZE_MAX and the loop happily indexes
//  data[SIZE_MAX], data[SIZE_MAX - 1], ... -- reads that left the buffer
//  long ago. And `len - 1` has already gone wrong before the loop starts if
//  len == 0: there is no -1, only SIZE_MAX.
//
//  The idiom that survives all of this is GOES-DOWN-TO -- test, then
//  decrement:
//
//      for (size_t i = len; i-- > 0;)            // len-1 first, 0 last,
//                                                // len == 0 never enters
//
//  Below, two backwards scans over a received frame. `find_last` was written
//  with a defensive `i > 0` -- which dodges the wrap and quietly never looks
//  at index 0. `count_trailing_padding` was written with `i >= 0`. One of
//  the tests plants a guard byte just before the buffer to show you exactly
//  where that scan ends up. Re-run the asan preset afterwards; the starter's
//  out-of-bounds reads light it up.
//
//  TASK
//    Fix both loops. The goes-down-to idiom fixes each in one line. Do not
//    change the tests.
//
//  RUN IT
//    ./mec test 01_02
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// Index of the LAST occurrence of `needle`, or -1 if it is absent.
ptrdiff_t find_last(const uint8_t *data, size_t len, uint8_t needle) {
  size_t i = len - 1; // TODO: for len == 0 this is already SIZE_MAX
  while (i > 0) {     // TODO: index 0 is never examined
    if (data[i] == needle) {
      return (ptrdiff_t)i;
    }
    --i;
  }
  return -1;
}

// How many bytes at the END of the buffer equal `pad`?
size_t count_trailing_padding(const uint8_t *data, size_t len, uint8_t pad) {
  size_t count = 0;
  for (size_t i = len - 1; i >= 0; --i) { // TODO: i >= 0 is always true
    if (data[i] != pad) {
      break;
    }
    ++count;
  }
  return count;
}

TEST("find_last prefers the last occurrence") {
  const uint8_t buf[] = {5, 9, 5, 7};
  CHECK_EQ(find_last(buf, sizeof buf, 5), 2);
}

TEST("find_last reaches index 0") {
  const uint8_t buf[] = {9, 1, 2, 3};
  CHECK_EQ(find_last(buf, sizeof buf, 9), 0);
}

TEST("find_last reports absence, including in an empty buffer") {
  const uint8_t buf[] = {1, 2, 3};
  CHECK_EQ(find_last(buf, sizeof buf, 9), -1);
  CHECK_EQ(find_last(buf, 0, 1), -1);
}

TEST("trailing padding is counted, and the count stops at the data") {
  const uint8_t frame[] = {0x10, 0x2A, 0xFF, 0xFF};
  CHECK_EQ(count_trailing_padding(frame, sizeof frame, 0xFF), 2u);
  CHECK_EQ(count_trailing_padding(frame, sizeof frame, 0x00), 0u);
}

TEST("an all-padding buffer is counted exactly once") {
  // The guard byte sits immediately BEFORE the buffer. A scan that runs off
  // the front finds more 0xFF there and keeps going -- the count comes back
  // bigger than the buffer. That is the underflow, made visible.
  struct {
    uint8_t guard;
    uint8_t buf[4];
  } t = {0xFF, {0xFF, 0xFF, 0xFF, 0xFF}};
  CHECK_EQ(count_trailing_padding(t.buf, sizeof t.buf, 0xFF), 4u);
}
