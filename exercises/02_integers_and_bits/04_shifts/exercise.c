// =============================================================================
//  02.04 -- Shifts: three ways to fall off the edge
// =============================================================================
//
//  Shifting looks like the safest operation in C. It has three separate ways
//  to be undefined behaviour (C17 6.5.7; CERT INT34-C):
//
//   1. Shifting by the width or more:      x << 32   (for a 32-bit x)
//   2. Shifting by a negative amount:      x << -1
//   3. Left-shifting a 1 into or past the sign bit of a SIGNED value:
//                                          1 << 31
//
//  The third is the one people refuse to believe. `1` is an int (02.01
//  again: everything small is an int), so `1 << 31` shifts a 1 into the
//  sign bit of a signed type -- UB. The one-character fix is `1u << 31`.
//
//  Why UB and not "whatever the instruction does"? Because the instructions
//  DISAGREE. On ARM, a 32-bit shift-by-40 shifts by 40 (result 0... for
//  register shifts it uses the low byte); on x86 the amount is taken mod 32,
//  so x << 40 is x << 8. C refuses to pick a winner, so the standard calls
//  it undefined -- and the optimiser exploits that freedom even on hardware
//  that would have been well-behaved.
//
//  The starter has all three lurking:
//
//   - `event_mask` builds a 64-bit mask with `1 << index`: the shift happens
//     in 32-bit INT arithmetic. index == 31 puts the 1 in the sign bit (UB),
//     and the negative int then SIGN-EXTENDS when widened to uint64_t --
//     0xFFFFFFFF80000000, a mask with 33 bits set. index >= 32 is UB by
//     width. Widen to the destination type BEFORE shifting.
//
//   - `rotl32` computes `x >> (32 - n)`: for n == 0 that is x >> 32.
//     (Rotation has no width problem once you handle that edge; mask n and
//     special-case 0.)
//
//   - `halve` uses `v >> 1` on a signed value. That is not UB -- it is
//     IMPLEMENTATION-DEFINED (the standard does not say whether the sign
//     bit smears in), and on every compiler you will meet it rounds toward
//     minus infinity, so `halve(-7)` says -4 where -7/2 is -3, and
//     `halve(-1)` is -1 for ever. Say `v / 2` when you mean division.
//
//  The asan preset (UBSan is part of it) reports 1 and 2 with line numbers.
//  Number 3 it cannot report, because nothing undefined happened -- only
//  something you probably did not mean.
//
//  TASK
//    Fix the three functions. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// A 64-slot event register; return the mask with only `index` set (index
// 0..63), or 0 for an out-of-range index.
uint64_t event_mask(unsigned index) {
  // TODO: the shift happens in 32-bit signed arithmetic; the cast arrives
  // after the damage is done (it was added to silence the compiler -- the
  // same story as 02.02, and the same lesson).
  return (uint64_t)(1 << index);
}

// Rotate a 32-bit value left by n (any n; rotation is modulo 32).
uint32_t rotl32(uint32_t x, unsigned n) {
  // TODO: what does the right-hand shift do when n == 0?
  return (x << n) | (x >> (32u - n));
}

// Half of a signed sensor delta, rounding toward zero like division does.
int32_t halve(int32_t v) {
  // TODO: this rounds toward minus infinity for negative v.
  return v >> 1;
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
