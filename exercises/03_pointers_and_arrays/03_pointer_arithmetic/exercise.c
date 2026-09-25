// =============================================================================
//  03.03 -- Pointer arithmetic and half-open ranges
// =============================================================================
//
//  Pointer arithmetic counts in ELEMENTS. `p + 1` on a uint32_t* moves four
//  bytes; on a struct pointer it moves sizeof(struct) bytes. The compiler
//  scales for you -- doing your own byte math on top of it is doing it
//  twice, and both bugs below are flavours of exactly that confusion.
//
//  The idiom this exercise drills is the HALF-OPEN RANGE, [begin, end):
//  `begin` is the first element, `end` is one past the last, and the loop
//  runs while `p != end`. Every parser, driver ring and standard-library
//  design uses it, because it composes: the empty range is begin == end, no
//  special case; splitting a range at p gives [begin, p) and [p, end) with
//  nothing counted twice.
//
//  The standard blesses it explicitly (C17 6.5.6p8): you may FORM a pointer
//  one past the end of an array and COMPARE against it. What you may not do
//  is DEREFERENCE it -- and a loop written `p <= end` does, on its final
//  lap. That is an out-of-bounds read (CERT ARR30-C): here it lands on the
//  sentinel the test planted after the buffer; on the target it lands on
//  whatever the linker put next.
//
//  Subtraction is arithmetic's mirror: `last - first` is the number of
//  ELEMENTS between two pointers into the same array, and its type is
//  ptrdiff_t (<stddef.h>) -- signed, since the answer may be negative.
//  Only pointers into the SAME array may be subtracted (ARR36-C); anything
//  else is undefined, not merely unhelpful. The starter's version casts to
//  byte pointers first, so it answers in bytes -- four times too many.
//
//  (Why a sentinel in a struct, rather than letting the sanitizer catch the
//  overread? Because an overread INSIDE an object -- one field into the
//  next -- is exactly what a sanitizer cannot see, and what a planted known
//  value can. Remember both tools; they cover different ground.)
//
//  TASK
//    Fix the loop bound in `sum_words` and the arithmetic in
//    `samples_between`. Do not change the tests.
//
//  RUN IT
//    ./mec test 03_03
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

uint32_t sum_words(const uint32_t *begin, const uint32_t *end) {
  uint32_t sum = 0;
  // TODO: `<=` sums one word past the end.
  for (const uint32_t *p = begin; p <= end; ++p) {
    sum += *p;
  }
  return sum;
}

size_t samples_between(const uint32_t *first, const uint32_t *last) {
  // TODO: bytes, not elements -- the compiler was already scaling.
  return (size_t)((const uint8_t *)last - (const uint8_t *)first);
}

TEST("sums exactly the half-open range") {
  struct {
    uint32_t data[4];
    uint32_t sentinel; // sits one past the end; must never be summed
  } buf = {{10, 20, 30, 40}, 0xDEADBEEFu};
  CHECK_EQ(sum_words(buf.data, buf.data + 4), 100u);
  CHECK_EQ(sum_words(buf.data, buf.data), 0u); // empty range: zero laps
}

TEST("distance between sample pointers is in elements, not bytes") {
  static const uint32_t samples[8] = {0};
  CHECK_EQ(samples_between(samples, samples + 8), 8u);
  CHECK_EQ(samples_between(samples + 2, samples + 5), 3u);
}
