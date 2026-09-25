// =============================================================================
//  00.03 -- Undefined behaviour, and why "it works" proves nothing
// =============================================================================
//
//  C's standard leaves some operations UNDEFINED: signed overflow, reading
//  past the end of an array, dereferencing NULL. When your program does one
//  of these, it has no meaning -- not "it crashes", not "you get a garbage
//  value", but anything at all. In practice that usually means: it appears
//  to work, on your machine, with your compiler, at this optimisation level,
//  until the day it does not. (Effective C ch. 1 draws the line between
//  implementation-defined, unspecified and undefined behaviour; UB is the
//  one you never get to reason about.)
//
//  Embedded folklore is full of the consequences. A firmware image that
//  worked for years fails when the vendor upgrades the compiler; the
//  optimiser started assuming -- as it is entitled to -- that the overflow
//  cannot happen, and deleted the check that caught it.
//
//  Two classics are below:
//
//  1. THE MIDPOINT THAT GOES NEGATIVE. `(lo + hi) / 2` overflows int when
//     `lo + hi` exceeds INT_MAX -- and a binary search over a large flash
//     address table gets exactly there. CERT INT32-C. The portable form
//     computes the *difference* first: `lo + (hi - lo) / 2`.
//
//  2. THE LOOP THAT WRITES ONE PAST THE END. `i <= size` instead of
//     `i < size` -- an off-by-one that clears one byte too many. CERT
//     ARR30-C. Here it lands on the struct member after the buffer, which
//     the test can see. On the real board it lands on whatever the linker
//     put there. The test uses a CANARY -- a byte with a known value placed
//     right after the buffer, checked afterwards. Keep that trick: it is how
//     stack-overflow detection works on MCUs, and you will build one in
//     chapter 08.
//
//  ONE MORE THING. This build runs at -O0 with no instrumentation, so the
//  bugs behave "predictably" -- today. The sanitizer build watches every
//  memory access and every signed operation at run time:
//
//      cmake --preset asan && ctest --preset asan
//
//  Run it after the tests pass here. It should stay quiet. Then try it on
//  the ORIGINAL starter code: it names both bugs, with file and line.
//  Learning to reach for it is part of the course -- it is the tool you have
//  on the host and do not have on the target, which is the argument for
//  running as much of your firmware logic on the host as you can.
//
//  TASK
//    Fix `flash_table_midpoint` and `wipe_key`. Do not change the tests.
//
//  RUN IT
//    ./mec test 00_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>
#include <string.h>

// Midpoint of two indices into a (large) table, lo <= hi.
int flash_table_midpoint(int lo, int hi) {
  // TODO: this overflows -- UB -- once lo + hi exceeds INT_MAX.
  return (lo + hi) / 2;
}

// Crypto hygiene: zero a key buffer after use.
void wipe_key(uint8_t *key, size_t size) {
  // TODO: one byte too many.
  for (size_t i = 0; i <= size; ++i) {
    key[i] = 0;
  }
}

TEST("midpoint of small values") {
  CHECK_EQ(flash_table_midpoint(0, 10), 5);
  CHECK_EQ(flash_table_midpoint(7, 8), 7);
}

TEST("midpoint of large values does not overflow") {
  // Two indices deep into a big table: lo + hi > INT_MAX.
  CHECK_EQ(flash_table_midpoint(2000000000, 2100000000), 2050000000);
}

TEST("wipe_key clears the key and nothing else") {
  struct {
    uint8_t key[16];
    uint8_t canary; // sits right after the buffer; must survive
  } slot;
  memset(slot.key, 0xAB, sizeof slot.key);
  slot.canary = 0x5A;

  wipe_key(slot.key, sizeof slot.key);

  for (size_t i = 0; i < sizeof slot.key; ++i) {
    CHECK_EQ(slot.key[i], 0u);
  }
  CHECK_EQ(slot.canary, 0x5Au); // the canary is the point of this test
}
