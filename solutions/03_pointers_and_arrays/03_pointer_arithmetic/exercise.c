// Solution -- 03.03 Pointer arithmetic and half-open ranges

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

uint32_t sum_words(const uint32_t *begin, const uint32_t *end) {
  uint32_t sum = 0;
  // `p != end` visits [begin, end): every element once, `end` never. The
  // one-past-the-end address is legal to FORM and COMPARE (C17 6.5.6p8) --
  // dereferencing it is the crime, and `<=` commits it on the last lap.
  for (const uint32_t *p = begin; p != end; ++p) {
    sum += *p;
  }
  return sum;
}

size_t samples_between(const uint32_t *first, const uint32_t *last) {
  // Pointer subtraction already scales: the result is in ELEMENTS, and its
  // type is ptrdiff_t -- signed, because `first - last` is a legal question
  // with a negative answer. The cast documents the precondition
  // (first <= last, same array); a debug assert would document it louder.
  return (size_t)(last - first);
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
