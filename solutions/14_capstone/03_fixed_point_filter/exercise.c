// Solution -- 14.03 The signal path: fixed-point filtering

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
  // The product is Q16.16 * Q0.16: up to 48 significant bits. Doing it in
  // 32 bits is the overflow the first-step test watches for.
  const int64_t scaled = (int64_t)alpha_q16 * ((int64_t)x - y);
  // Round to nearest, not truncate: >> alone biases every step downward
  // by up to one LSB, and a filter is a machine for accumulating biases.
  const int32_t step = (int32_t)((scaled + 32768) >> 16);
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
  CHECK_EQ(q16_to_mv_i16(65540800), 1000);          // 1000.073 mV -> 1000
  CHECK_EQ(q16_to_mv_i16(999 * 65536 + 32768), 1000); // exactly .5: up
  CHECK_EQ(q16_to_mv_i16(999 * 65536 + 32767), 999);
  CHECK_EQ(q16_to_mv_i16(-65536), -1);
  CHECK_EQ(q16_to_mv_i16(INT32_MAX), INT16_MAX); // 32768.0 mV saturates
  CHECK_EQ(q16_to_mv_i16(INT32_MIN), INT16_MIN);
}
