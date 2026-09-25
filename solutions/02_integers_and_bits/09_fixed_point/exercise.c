// Solution -- 02.09 Fixed point: fractions without an FPU

#include <mect/mect.h>

#include <stdint.h>

typedef int32_t q16; // Q16.16: 16 integer bits, 16 fraction bits

enum { Q16_ONE = 65536 };

q16 q16_mul(q16 a, q16 b) {
  // The product of two Q16.16 values is Q32.32: it needs 64 bits before
  // the renormalising shift, no exceptions. Adding half of the discarded
  // weight (1 << 15) before the shift rounds to nearest instead of
  // truncating toward minus infinity -- over a filter's thousands of
  // multiplies, truncation is a systematic drift, not noise.
  //
  // (The >> of a negative value is implementation-defined in C17; it is an
  // arithmetic shift on every compiler this course targets, and MISRA's
  // ask is that you write that assumption down. This comment is that.)
  return (q16)((((int64_t)a * b) + (1 << 15)) >> 16);
}

q16 q16_from_ratio(int32_t num, int32_t den) {
  // Widen BEFORE the shift: num << 16 must happen in 64-bit arithmetic or
  // any |num| >= 32768 overflows. den/2 rounds the division to nearest
  // (for positive den).
  return (q16)((((int64_t)num << 16) + den / 2) / den);
}

int32_t q16_to_centi(q16 v) {
  // Same shape again: widen, scale, round, shift.
  return (int32_t)(((int64_t)v * 100 + (1 << 15)) >> 16);
}

TEST("multiply: integers, fractions, negatives") {
  CHECK_EQ(q16_mul(3 * Q16_ONE, 4 * Q16_ONE), 12 * Q16_ONE);
  CHECK_EQ(q16_mul(3 * Q16_ONE / 2, 5 * Q16_ONE / 2), // 1.5 * 2.5
           15 * Q16_ONE / 4);                         // = 3.75
  CHECK_EQ(q16_mul(-3 * Q16_ONE / 2, 2 * Q16_ONE), -3 * Q16_ONE);
}

TEST("multiply rounds to nearest, not toward zero") {
  // Smallest positive value times one half: 0.5 ulp must round UP to 1 ulp.
  CHECK_EQ(q16_mul(1, Q16_ONE / 2), 1);
}

TEST("ratios: pi to a part in ten thousand") {
  const q16 pi = q16_from_ratio(355, 113);
  CHECK_EQ(pi, 205887); // floor(355/113 * 65536 + 0.5)
  CHECK_NEAR((double)pi / (double)Q16_ONE, 3.14159265, 1e-4);
}

TEST("ratios that overflow 32-bit intermediates") {
  // 40000/2 = 20000, comfortably representable -- but 40000 << 16 is not,
  // unless the shift happens in 64-bit arithmetic.
  CHECK_EQ(q16_from_ratio(40000, 2), 20000 * Q16_ONE);
}

TEST("centidegrees for the display") {
  CHECK_EQ(q16_to_centi(36 * Q16_ONE + Q16_ONE / 2), 3650); // 36.50 C
  CHECK_EQ(q16_to_centi(400 * Q16_ONE), 40000); // the 32-bit trap: 400 C
  CHECK_EQ(q16_to_centi(328), 1); // 0.005 C rounds up to 1 centidegree
  CHECK_EQ(q16_to_centi(0), 0);
}
