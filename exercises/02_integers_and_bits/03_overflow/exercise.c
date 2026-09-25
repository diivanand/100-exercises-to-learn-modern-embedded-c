// =============================================================================
//  02.03 -- Signed overflow is not a wrap, it is a promise broken
// =============================================================================
//
//  Unsigned arithmetic wraps modulo 2^N by definition. Signed arithmetic
//  does not wrap: exceeding the range of a signed type is UNDEFINED
//  BEHAVIOUR (00.03), and the optimiser is allowed to assume it never
//  happens. `a + b < a` as an "overflow check" compiles to `false` on a
//  modern compiler, and the branch it guarded goes with it. (Effective C
//  ch. 3; CERT INT32-C.)
//
//  So overflow has to be headed off BEFORE the operation, in arithmetic
//  that cannot itself overflow:
//
//      b > 0 && a > INT32_MAX - b     // a + b would exceed INT32_MAX
//      b < 0 && a < INT32_MIN - b     // a + b would fall below INT32_MIN
//
//  Study those two lines until they are obvious; they are the whole trick,
//  and the multiplication and subtraction versions are built the same way.
//
//  WHAT SHOULD A FIRMWARE DO when the sum genuinely does not fit? Crashing
//  is rarely on the menu. The embedded answer is usually SATURATION: peg the
//  value at the nearest rail, the way an analogue meter does. A pegged
//  reading is wrong by a bounded amount in a known direction; a wrapped one
//  says a fully-charged battery is empty. DSP instruction sets (including
//  the Cortex-M4's QADD) saturate in hardware for exactly this reason.
//
//  The starter's meter does the classic thing instead: it just adds. Two
//  1.6 GJ readings later, the total is negative. (That is the WRAPPED
//  result; because reaching it was UB, the compiler was also within its
//  rights to do something stranger. The asan preset -- UBSan is in it --
//  reports the exact line.)
//
//  TASK
//    Implement `checked_add` (precondition form, no UB, `*sum` written only
//    on success), build `sat_add` on it, and make `energy_total` saturate.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 02_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Adds a + b into *sum and returns true, or returns false (leaving *sum
// alone) if the sum does not fit in int32_t.
bool checked_add(int32_t a, int32_t b, int32_t *sum) {
  // TODO: this version commits the overflow it was asked to prevent.
  *sum = a + b;
  return true;
}

// a + b, pegged to INT32_MAX / INT32_MIN instead of overflowing.
int32_t sat_add(int32_t a, int32_t b) {
  // TODO: same problem.
  return a + b;
}

int32_t energy_total(const int32_t *samples, size_t n) {
  int32_t total = 0;
  for (size_t i = 0; i < n; ++i) {
    // TODO: the meter should peg, not wrap.
    total += samples[i];
  }
  return total;
}

TEST("checked_add accepts sums that fit") {
  int32_t sum = 0;
  CHECK(checked_add(100, -200, &sum));
  CHECK_EQ(sum, -100);
  CHECK(checked_add(INT32_MAX, 0, &sum));
  CHECK_EQ(sum, INT32_MAX);
  CHECK(checked_add(INT32_MIN, INT32_MAX, &sum));
  CHECK_EQ(sum, -1);
}

TEST("checked_add refuses sums that do not") {
  int32_t sum = 0;
  CHECK_FALSE(checked_add(INT32_MAX, 1, &sum));
  CHECK_FALSE(checked_add(2000000000, 2000000000, &sum));
  CHECK_FALSE(checked_add(INT32_MIN, -1, &sum));
  CHECK_FALSE(checked_add(-2000000000, -2000000000, &sum));
}

TEST("sat_add pegs at the rails") {
  CHECK_EQ(sat_add(5, 7), 12);
  CHECK_EQ(sat_add(2000000000, 2000000000), INT32_MAX);
  CHECK_EQ(sat_add(-2000000000, -2000000000), INT32_MIN);
  CHECK_EQ(sat_add(INT32_MAX, INT32_MIN), -1);
}

TEST("an energy meter pegs instead of going negative") {
  // Two 1.6 GJ readings: the true sum (3.2e9) does not fit in int32_t.
  const int32_t big[] = {1600000000, 1600000000};
  CHECK_EQ(energy_total(big, 2), INT32_MAX);

  const int32_t normal[] = {5, -3, 10};
  CHECK_EQ(energy_total(normal, 3), 12);
}
