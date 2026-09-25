// =============================================================================
//  02.09 -- Fixed point: fractions without an FPU
// =============================================================================
//
//  How does a part with no floating-point hardware do 36.5 degrees? It
//  scales: store TIMES 65536 and remember where the point is. That is
//  Q16.16 -- 16 integer bits, 16 fraction bits, in a plain int32_t:
//
//      1.0     ->  65536          0.5   -> 32768
//      3.75    ->  245760         -3.0  -> -196608
//
//  Addition and comparison work UNCHANGED -- that is the beauty of it.
//  Multiplication needs care: (a * 65536) * (b * 65536) carries 65536
//  squared, so the raw product is a Q32.32 that needs 64 BITS, and must be
//  shifted back down by 16. The whole discipline is three rules:
//
//   1. WIDEN FIRST. The product of two Q16.16 values needs int64_t, always
//      (02.03: the overflow is UB, and it is also just wrong). Same for
//      `num << 16` in the ratio constructor: 40000 << 16 does not fit.
//
//   2. ROUND, DO NOT TRUNCATE. Add half of what the shift will discard
//      (1 << 15) before shifting. A truncating multiply loses up to a full
//      ulp ALWAYS IN THE SAME DIRECTION; run a filter for an hour and the
//      drift walks your baseline. (This is why the tests pin exact values.)
//
//   3. KNOW YOUR RANGE. Q16.16 spans about +/-32768 with ~1.5e-5
//      resolution. Scaling to centidegrees multiplies by 100 -- which is
//      why `q16_to_centi` needs the 64-bit intermediate too: 400 degrees
//      times 100 times 65536 is over 2^31.
//
//  (Effective C ch. 3 covers the floating-point model this replaces. For
//  the record: the STM32L476's Cortex-M4F has a single-precision FPU, so
//  `float` is genuinely cheap there -- but `double` is still software, and
//  1.0 is a double (00.02). Fixed point remains the lingua franca for
//  filters, control loops and anything that must be bit-exact across
//  builds.)
//
//  The starter does everything in int32_t and truncates. Every test below
//  it fails, each for a reason worth reading.
//
//  TASK
//    Apply the three rules to the three functions. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_09
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

typedef int32_t q16; // Q16.16: 16 integer bits, 16 fraction bits

enum { Q16_ONE = 65536 };

q16 q16_mul(q16 a, q16 b) {
  // TODO: the product of two Q16.16 values does not fit in 32 bits, and
  // truncation drifts.
  return (q16)((a * b) >> 16);
}

q16 q16_from_ratio(int32_t num, int32_t den) {
  // TODO: num << 16 overflows for |num| >= 32768, and no rounding.
  return (num << 16) / den;
}

int32_t q16_to_centi(q16 v) {
  // TODO: v * 100 overflows for values a thermocouple actually reaches.
  return (v * 100) >> 16;
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
