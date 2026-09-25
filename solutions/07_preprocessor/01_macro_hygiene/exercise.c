// Solution -- 07.01 Macro hygiene: parentheses, and knowing when to stop

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// Parenthesise every parameter AND the whole definition (CERT PRE01-C and
// PRE02-C). The parameter parentheses defend against operators INSIDE the
// argument; the outer pair defends against operators AROUND the expansion.
#define CALIBRATE(raw) (((raw) * 4) + 1)

// min-as-a-macro cannot be repaired: however many parentheses it wears, it
// still evaluates one argument twice (CERT PRE31-C). The cure is not a
// better macro but a function. `static inline` gives the compiler the same
// inlining opportunity the macro pretended to offer -- with types checked,
// arguments evaluated exactly once, and a name the debugger knows.
static inline uint32_t min_u32(uint32_t a, uint32_t b) {
  return a < b ? a : b;
}

TEST("calibration survives an expression argument") {
  const int a = 2;
  const int b = 3;
  // (2 + 3) * 4 + 1 = 21. The unparenthesised macro computed 2 + 3*4 + 1.
  CHECK_EQ(CALIBRATE(a + b), 21);
}

TEST("calibration survives an expression around the call") {
  // 10 - CALIBRATE(2) = 10 - 9 = 1. Without the outer parentheses the
  // expansion was 10 - 2*4 + 1 = 3.
  CHECK_EQ(10 - CALIBRATE(2), 1);
}

TEST("min evaluates each argument exactly once") {
  uint32_t samples[] = {3, 7};
  size_t i = 0;
  const uint32_t m = min_u32(samples[i++], 5u);
  CHECK_EQ(m, 3u);        // min(3, 5)
  CHECK_EQ(i, (size_t)1); // i++ must have happened once, not twice
}

TEST("min still behaves like min") {
  CHECK_EQ(min_u32(5u, 9u), 5u);
  CHECK_EQ(min_u32(9u, 5u), 5u);
  CHECK_EQ(min_u32(4u, 4u), 4u);
}
