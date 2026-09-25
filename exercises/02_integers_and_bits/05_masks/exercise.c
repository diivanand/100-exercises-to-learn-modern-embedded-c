// =============================================================================
//  02.05 -- Masks and fields: the vocabulary of register work
// =============================================================================
//
//  Half of embedded C is reading and writing FIELDS -- named runs of bits
//  inside a wider word. The entire vocabulary is four idioms:
//
//      v |=  mask;      // set the bits in mask
//      v &= ~mask;      // clear them
//      v ^=  mask;      // toggle them
//      (v & mask)       // test them  (see below for the subtlety)
//
//  plus two derived moves for multi-bit fields at position `pos`, width
//  `width`:
//
//      (v >> pos) & ((1u << width) - 1)              // extract
//      (v & ~(m << pos)) | ((val & m) << pos)        // replace
//
//  Two classic mistakes are waiting in the starter:
//
//  1. "ALL BITS SET" TESTED WITH TRUTHINESS. `if (v & mask)` answers "is at
//     least ONE of them set?". When the question is "are they ALL set?" --
//     two ready flags, say -- you need `(v & mask) == mask`. The starter's
//     `bits_all` gets this wrong, and returns true for a half-ready device.
//
//  2. OR-ONLY UPDATE. The starter's `field_set` ORs the new value over the
//     old one without clearing first. OR can only turn bits ON, so the
//     update works on a fresh register -- every test you ran on day one --
//     and corrupts the field the first time it REPLACES a nonzero value.
//     Mode 1 OR mode 2 is mode 3, and mode 3 is analogue. This exact bug,
//     on a real MODER register, is chapter 11's opening act; learn it here
//     on values, where it cannot fry anything.
//
//  There is also an edge in `mask_n`: the shift trick `(1u << n) - 1`
//  cannot produce the n == 32 mask, because 1u << 32 is UB (02.04). Guard
//  the edge explicitly. (CERT INT34-C again; Effective C ch. 4 covers the
//  bitwise operators.)
//
//  TASK
//    Fix `bits_all`, `mask_n` and `field_set`. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// The four idioms. These are worth having as named functions once, so the
// rest of a code base reads as intent instead of punctuation.
uint32_t bits_set(uint32_t v, uint32_t mask) {
  return v | mask;
}

uint32_t bits_clear(uint32_t v, uint32_t mask) {
  return v & ~mask;
}

uint32_t bits_toggle(uint32_t v, uint32_t mask) {
  return v ^ mask;
}

bool bits_all(uint32_t v, uint32_t mask) {
  // TODO: this answers "any", not "all".
  return (v & mask) != 0;
}

bool bits_any(uint32_t v, uint32_t mask) {
  return (v & mask) != 0;
}

uint32_t mask_n(unsigned n) {
  // TODO: what happens for n == 32? (02.04 has the answer.)
  return (1u << n) - 1u;
}

uint32_t field_get(uint32_t v, unsigned pos, unsigned width) {
  return (v >> pos) & mask_n(width);
}

uint32_t field_set(uint32_t v, unsigned pos, unsigned width, uint32_t val) {
  // TODO: OR can only turn bits on. Updating a nonzero field corrupts it.
  return v | ((val & mask_n(width)) << pos);
}

TEST("set, clear, toggle") {
  CHECK_EQ(bits_set(0x00F0u, 0x000Fu), 0x00FFu);
  CHECK_EQ(bits_clear(0x00FFu, 0x000Fu), 0x00F0u);
  CHECK_EQ(bits_toggle(0x00FFu, 0x0110u), 0x01EFu);
}

TEST("all vs any") {
  CHECK(bits_all(0x00F0u, 0x0030u));
  CHECK_FALSE(bits_all(0x0050u, 0x0030u)); // one of the two bits: not all
  CHECK(bits_any(0x0050u, 0x0030u));
  CHECK_FALSE(bits_any(0x0050u, 0x000Fu));
}

TEST("mask_n across its range") {
  CHECK_EQ(mask_n(0), 0u);
  CHECK_EQ(mask_n(1), 1u);
  CHECK_EQ(mask_n(8), 0xFFu);
  CHECK_EQ(mask_n(32), 0xFFFFFFFFu); // the edge the shift trick cannot reach
}

TEST("field extract") {
  const uint32_t moder = 0x00000A40u;
  CHECK_EQ(field_get(moder, 6, 2), 1u);  // pin 3: mode 1
  CHECK_EQ(field_get(moder, 8, 2), 2u);  // pin 4: mode 2
  CHECK_EQ(field_get(moder, 10, 2), 2u); // pin 5: mode 2
}

TEST("field update REPLACES the old value") {
  uint32_t v = 0;
  v = field_set(v, 10, 2, 1u); // pin 5 -> mode 1 (output)
  CHECK_EQ(v, 0x400u);
  v = field_set(v, 10, 2, 2u); // pin 5 -> mode 2 (alternate function)
  CHECK_EQ(v, 0x800u);         // NOT 0xC00: mode 1 must be gone
  v = field_set(v, 10, 2, 0u); // and back to input
  CHECK_EQ(v, 0u);
}
