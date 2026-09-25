// Solution -- 03.04 const placement: promises read right to left

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// `const int16_t *table`: pointer to CONST data -- "I will look, not touch".
// That one word is what lets a caller hand us a table that lives in flash.
// The fix is in the SIGNATURE, never in a cast at the call site: a cast
// silences the compiler and keeps the lie (-Wcast-qual exists to catch it,
// and writing through it to a genuinely const object is undefined -- on the
// target, a bus fault or a silent nothing, depending on the flash
// controller's mood).
int16_t cal_lookup(const int16_t *table, size_t len, size_t idx) {
  if (idx >= len) {
    idx = len - 1; // clamp: a sensor index off the end reads the last entry
  }
  return table[idx];
}

int16_t cal_min(const int16_t *table, size_t len) {
  int16_t min = table[0];
  for (size_t i = 1; i < len; ++i) {
    if (table[i] < min) {
      min = table[i];
    }
  }
  return min;
}

static const int16_t cal[] = {-120, -60, 0, 60, 120};

TEST("lookup reads the const calibration table") {
  CHECK_EQ(cal_lookup(cal, 5, 2), 0);
  CHECK_EQ(cal_lookup(cal, 5, 0), -120);
  CHECK_EQ(cal_lookup(cal, 5, 99), 120); // clamped to the last entry
}

TEST("min scans the const table") {
  CHECK_EQ(cal_min(cal, 5), -120);
}
