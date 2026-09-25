// =============================================================================
//  14.03 -- The signal path: fixed-point filtering
// =============================================================================
//
//  Layer three of the command station: the ADC readings that CMD_READ_SENSOR
//  will report are noisy, and the cure is a filter. On a part with no FPU --
//  or one you refuse to pay for in the interrupt path -- filters run in
//  fixed point. This is 02.09's arithmetic doing a real job.
//
//  THE FILTER. The exponential moving average is the workhorse smoother of
//  embedded telemetry, one multiply and one add per sample and no history
//  buffer:
//
//      y += alpha * (x - y)          0 < alpha < 1
//
//  In our Q formats: y and x are Q16.16 millivolts (upper 16 bits whole
//  millivolts, lower 16 bits fraction), alpha is a Q0.16 fraction --
//  16384 means 0.25. Two things about that multiply, and they are the two
//  bugs waiting below:
//
//   1. WIDTH. (x - y) spans the full signal range: up to 3300 mV in Q16.16
//      is 216,268,800 -- 2^27.7. Times alpha (up to 2^16) is a 44-bit
//      product. `int32_t * int32_t` keeps 32 of those and throws UB on the
//      rest (02.03). The multiply must go through int64_t -- one cast, done
//      BEFORE the multiply, not after (02.01's lesson at a larger size).
//
//   2. ROUNDING. `>> 16` truncates toward minus infinity: every step comes
//      out up to one LSB smaller than the mathematics says. A filter is a
//      loop that runs forever, i.e. a machine for accumulating exactly such
//      biases -- the settling test can SEE the truncated version park three
//      LSBs below the target instead of one. Add half (32768) before the
//      shift: round to nearest.
//
//  AN HONEST LIMIT. Even rounded, an EMA in fixed point has a DEADBAND:
//  once alpha * (x - y) rounds to zero, y stops moving -- here, within one
//  LSB of the target, which is 1/65536 of a millivolt and nobody's problem.
//  Know that it exists; the steady-state test pins it down (settle, then a
//  thousand more samples, not one LSB of drift).
//
//  The conversion helpers at either end -- ADC counts in, saturated int16
//  wire value out (02.03's saturation, 02.09's rounding) -- are given.
//
//  TASK
//    Fix `ema_step`: widen the multiply, round the shift. Do not change
//    the tests.
//
//  RUN IT
//    ./mec test 14_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// --- given: ADC counts to Q16.16 millivolts (00.01 upgraded via 02.09) --------

int32_t q16_mv_from_adc(uint16_t raw) {
  // raw * 3300 needs 24 bits; << 16 pushes it past 32. Widen FIRST.
  return (int32_t)(((int64_t)raw * 3300 << 16) / 4095);
}

// --- the filter -----------------------------------------------------------------

// One step of an exponential moving average, y += alpha * (x - y), with
// alpha a Q0.16 fraction (16384 = 0.25). All Q16.16 in, Q16.16 out.
int32_t ema_step(int32_t y, int32_t x, uint32_t alpha_q16) {
  // TODO: two bugs. This product is computed in 32 bits (it needs 44),
  // and the shift truncates instead of rounding.
  const int32_t step = (int32_t)(((int32_t)alpha_q16 * (x - y)) >> 16);
  return y + step;
}

// --- given: Q16.16 millivolts to a rounded, saturated wire value ---------------

int16_t q16_to_mv_i16(int32_t q16) {
  const int32_t mv = (int32_t)(((int64_t)q16 + 32768) >> 16);
  if (mv > INT16_MAX) {
    return INT16_MAX;
  }
  if (mv < INT16_MIN) {
    return INT16_MIN;
  }
  return (int16_t)mv;
}

// --- tests ----------------------------------------------------------------------

#define ALPHA_QUARTER 16384u // 0.25 in Q0.16

TEST("adc conversion is exact where it can be") {
  CHECK_EQ(q16_mv_from_adc(0), 0);
  CHECK_EQ(q16_mv_from_adc(4095), 3300 * 65536); // full scale: exact
  // 1241 counts = 1000.0733 mV; in Q16.16 that is 65540801 (verified
  // against (1241 * 3300 * 65536) / 4095 done in 64-bit).
  CHECK_EQ(q16_mv_from_adc(1241), 65540801);
}

TEST("the first step from cold start is exact -- and huge") {
  // y = 0, x = full scale. alpha * x = 0.25 * 216268800 = 54067200. A
  // 32-bit product wraps long before this and produces rubbish.
  const int32_t x = q16_mv_from_adc(4095);
  CHECK_EQ(ema_step(0, x, ALPHA_QUARTER), 54067200);
}

TEST("the filter settles within one LSB of a constant input") {
  const int32_t target = q16_mv_from_adc(1241); // 65540801
  int32_t y = 0;
  for (int i = 0; i < 200; ++i) {
    y = ema_step(y, target, ALPHA_QUARTER);
  }
  // Round-to-nearest parks within 1 LSB below the target (the step
  // rounds to zero once alpha * diff < half an LSB). Truncation parks
  // 3 LSB short with this alpha -- which is what this bound catches.
  CHECK(y <= target);
  CHECK(y >= target - 1);
}

TEST("steady state holds: no limit cycle, no drift") {
  const int32_t target = q16_mv_from_adc(2048);
  int32_t y = 0;
  for (int i = 0; i < 200; ++i) {
    y = ema_step(y, target, ALPHA_QUARTER);
  }
  const int32_t settled = y;
  for (int i = 0; i < 1000; ++i) {
    y = ema_step(y, target, ALPHA_QUARTER);
  }
  CHECK_EQ(y, settled);
}

TEST("wire conversion rounds to nearest and saturates at the rails") {
  CHECK_EQ(q16_to_mv_i16(65540800), 1000);            // 1000.073 mV -> 1000
  CHECK_EQ(q16_to_mv_i16(999 * 65536 + 32768), 1000); // exactly .5: up
  CHECK_EQ(q16_to_mv_i16(999 * 65536 + 32767), 999);
  CHECK_EQ(q16_to_mv_i16(-65536), -1);
  CHECK_EQ(q16_to_mv_i16(INT32_MAX), INT16_MAX); // 32768.0 mV saturates
  CHECK_EQ(q16_to_mv_i16(INT32_MIN), INT16_MIN);
}
