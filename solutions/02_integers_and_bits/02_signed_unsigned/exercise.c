// Solution -- 02.02 Signed meets unsigned: the conversion you did not order

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t uart_space_left(size_t capacity, size_t used) {
  // Check BEFORE subtracting. Unsigned subtraction cannot go negative; it
  // wraps (CERT INT30-C), and a "space left" of 18 quintillion admits every
  // write that follows.
  if (used >= capacity) {
    return 0;
  }
  return capacity - used;
}

bool fits(int needed, size_t space) {
  // The sign check comes FIRST; only then is the cast meaningful. The
  // compiler's mixed-comparison warning was never the problem -- the
  // negative value reaching this function was, and the cast that silenced
  // the warning silenced the evidence.
  if (needed < 0) {
    return false;
  }
  return (size_t)needed <= space;
}

uint32_t sum_all(const uint8_t *data, size_t n) {
  uint32_t sum = 0;
  // `i < n` holds for every n, including 0. The starter's `i <= n - 1`
  // needed n - 1 to exist, and for n == 0 it is SIZE_MAX.
  for (size_t i = 0; i < n; ++i) {
    sum += data[i];
  }
  return sum;
}

TEST("space left in a buffer") {
  CHECK_EQ(uart_space_left(64, 10), 54u);
  CHECK_EQ(uart_space_left(64, 64), 0u);
}

TEST("space left when an ISR overshot the high-water mark") {
  // `used` can legitimately pass `capacity` for a moment in this design;
  // the answer is 0, not 2^64 - 6.
  CHECK_EQ(uart_space_left(64, 70), 0u);
}

TEST("fits() with honest sizes") {
  CHECK(fits(50, 100));
  CHECK(fits(100, 100));
  CHECK_FALSE(fits(200, 100));
  CHECK(fits(0, 0));
}

TEST("fits() when an error code leaks in") {
  // -1 cast to size_t is SIZE_MAX -- "fits anywhere". It must not.
  CHECK_FALSE(fits(-1, 100));
}

TEST("summing zero bytes is zero, not a walk off the end") {
  const uint8_t data[] = {1, 2, 3};
  CHECK_EQ(sum_all(data, 3), 6u);
  CHECK_EQ(sum_all(data, 0), 0u);
}
