// Solution -- 02.03 Signed overflow is not a wrap, it is a promise broken

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool checked_add(int32_t a, int32_t b, int32_t *sum) {
  // Test the PRECONDITION, in arithmetic that cannot itself overflow: this
  // is CERT INT32-C's canonical form. `INT32_MAX - b` is safe because b > 0
  // there, and `INT32_MIN - b` is safe because b < 0 there.
  if (b > 0 && a > INT32_MAX - b) {
    return false;
  }
  if (b < 0 && a < INT32_MIN - b) {
    return false;
  }
  *sum = a + b; // now provably in range
  return true;
}

int32_t sat_add(int32_t a, int32_t b) {
  int32_t sum;
  if (checked_add(a, b, &sum)) {
    return sum;
  }
  // Peg at the rail we ran into. b's sign says which one (b == 0 never
  // overflows, so it cannot reach here).
  return b > 0 ? INT32_MAX : INT32_MIN;
}

int32_t energy_total(const int32_t *samples, size_t n) {
  int32_t total = 0;
  for (size_t i = 0; i < n; ++i) {
    // Saturating accumulate: a pegged meter reading is wrong by a known,
    // bounded amount and in a known direction. A wrapped one is arbitrary
    // -- and reaching it was undefined behaviour anyway.
    total = sat_add(total, samples[i]);
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
