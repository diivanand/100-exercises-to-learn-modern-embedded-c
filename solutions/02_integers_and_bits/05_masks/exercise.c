// Solution -- 02.05 Masks and fields: the vocabulary of register work

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
  // Equality against the mask, not truthiness of the AND: `v & mask` is
  // nonzero as soon as ONE bit matches.
  return (v & mask) == mask;
}

bool bits_any(uint32_t v, uint32_t mask) {
  return (v & mask) != 0;
}

uint32_t mask_n(unsigned n) {
  // The n == 32 guard: 1u << 32 is UB (02.04), so the all-bits case cannot
  // be produced by the shift-and-subtract trick.
  if (n >= 32) {
    return UINT32_MAX;
  }
  return (1u << n) - 1u;
}

uint32_t field_get(uint32_t v, unsigned pos, unsigned width) {
  return (v >> pos) & mask_n(width);
}

uint32_t field_set(uint32_t v, unsigned pos, unsigned width, uint32_t val) {
  const uint32_t m = mask_n(width);
  // CLEAR the field, then OR the new value in. OR alone can only turn bits
  // on: it works the first time (the field was zero) and never again.
  return (v & ~(m << pos)) | ((val & m) << pos);
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
  // A GPIO MODER-style register: 2-bit fields, field i at bits 2i+1..2i.
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
