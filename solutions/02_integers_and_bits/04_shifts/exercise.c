// Solution -- 02.04 Shifts: three ways to fall off the edge

#include <mect/mect.h>

#include <stdint.h>

uint64_t event_mask(unsigned index) {
  if (index >= 64) {
    return 0; // out of range: no bits, rather than UB
  }
  // Widen FIRST, then shift. `1` is an int; `(uint64_t)1 << index` performs
  // the shift at the width the result needs, with an unsigned type, so
  // neither the >= 32 case nor the sign bit can bite.
  return (uint64_t)1 << index;
}

uint32_t rotl32(uint32_t x, unsigned n) {
  n &= 31u; // rotation is modulo the width...
  if (n == 0) {
    return x; // ...and the n == 0 case must not become x >> 32
  }
  return (x << n) | (x >> (32u - n));
}

int32_t halve(int32_t v) {
  // Division states the intent and rounds toward zero, like every other
  // C division. `v >> 1` on a negative value is implementation-defined and
  // -- where it is arithmetic shift, i.e. everywhere you will ever port to
  // -- rounds toward MINUS INFINITY: -7 >> 1 is -4, and -1 >> 1 is -1 for
  // ever. The compiler emits the same shift for /2 where it is safe.
  return v / 2;
}

TEST("event masks across the whole range") {
  CHECK_EQ(event_mask(0), 1ull);
  CHECK_EQ(event_mask(5), 32ull);
  CHECK_EQ(event_mask(31), 0x80000000ull); // the sign-bit trap lives here
  CHECK_EQ(event_mask(40), 1ull << 40);    // and the width trap here
  CHECK_EQ(event_mask(64), 0ull);
}

TEST("rotate left, including by zero") {
  CHECK_EQ(rotl32(0x80000001u, 1), 3u);
  CHECK_EQ(rotl32(0x12345678u, 16), 0x56781234u);
  CHECK_EQ(rotl32(0xDEADBEEFu, 0), 0xDEADBEEFu); // n == 0: no x >> 32 allowed
  CHECK_EQ(rotl32(0xDEADBEEFu, 32), 0xDEADBEEFu);
}

TEST("halving negative readings rounds toward zero") {
  CHECK_EQ(halve(8), 4);
  CHECK_EQ(halve(7), 3);
  CHECK_EQ(halve(-7), -3); // v >> 1 would say -4
  CHECK_EQ(halve(-1), 0);  // v >> 1 would say -1, for ever
}
