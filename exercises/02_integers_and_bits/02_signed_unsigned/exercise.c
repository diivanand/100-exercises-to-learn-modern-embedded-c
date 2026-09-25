// =============================================================================
//  02.02 -- Signed meets unsigned: the conversion you did not order
// =============================================================================
//
//  When an operator sees one signed and one unsigned operand of the same
//  rank, the SIGNED one is converted to unsigned. Not the other way round.
//  So:
//
//      -1 > 0u                     // true: -1 becomes 0xFFFFFFFF
//      (size_t)0 - 1               // SIZE_MAX, by definition -- unsigned
//                                  // arithmetic wraps, it cannot go negative
//
//  (Effective C ch. 3, "Usual Arithmetic Conversions"; CERT INT02-C. The
//  wrap itself is DEFINED behaviour -- CERT INT30-C is about wrapping when
//  you did not mean to -- which makes it worse in a way: no sanitizer will
//  flag it, because nothing undefined happened. Only your tests can.)
//
//  Three habitats of the bug, all below:
//
//  1. SUBTRACT THEN COMPARE. `capacity - used` is a fine expression until
//     the day `used` is bigger -- here, an ISR is allowed to overshoot a
//     soft high-water mark for one write. The difference wraps to nearly
//     2^64 and every subsequent "is there room?" check answers yes. Compare
//     first, subtract after.
//
//  2. THE CAST THAT SILENCES THE WARNING. This build makes mixed
//     signed/unsigned comparisons an error, so somebody "fixed" `fits` with
//     a cast. Now `fits(-1, ...)` -- an error code that leaked into a size
//     parameter -- converts to SIZE_MAX and fits everywhere. The warning was
//     pointing at a real question ("what if it is negative?"); the cast
//     answered it with "then pretend it is huge". Check the sign FIRST, then
//     cast.
//
//  3. THE LOOP BOUND. `i <= n - 1` is `i < n` for every n except the one
//     that matters: n == 0 makes `n - 1` SIZE_MAX and the loop reads until
//     something stops it. On the host that is a crash if you are lucky; on
//     an MCU it is a silent walk through the whole address space. Write
//     `i < n`.
//
//  A style rule follows from all this: sizes and indices are size_t
//  EVERYWHERE, so comparisons stay same-signed. The signed/unsigned frontier
//  should be a checkpoint you built on purpose (like `fits`), not a surprise
//  inside an expression.
//
//  TASK
//    Fix the three functions. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_02
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t uart_space_left(size_t capacity, size_t used) {
  // TODO: when used > capacity this wraps to nearly SIZE_MAX.
  return capacity - used;
}

bool fits(int needed, size_t space) {
  // TODO: the cast was added to silence the mixed-comparison error. For
  // needed == -1 it manufactures SIZE_MAX.
  return (size_t)needed <= space;
}

uint32_t sum_all(const uint8_t *data, size_t n) {
  uint32_t sum = 0;
  // TODO: what is n - 1 when n == 0?
  for (size_t i = 0; i <= n - 1; ++i) {
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
