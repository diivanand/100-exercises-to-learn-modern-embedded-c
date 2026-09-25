// =============================================================================
//  05.03 -- Unions and type punning: reading bits lawfully
// =============================================================================
//
//  A union's members all start at offset zero of the same storage, and its
//  size is its largest member's. Write one member, read another, and you get
//  the stored bytes REINTERPRETED as the other type. In C -- unlike C++ --
//  that reinterpretation is sanctioned: C17 6.5.2.3 (footnote 107) says the
//  bytes are reread as the new type. It is the language's blessed way to ask
//  "what bits is this float made of", short of copying them out.
//
//  Embedded code really does ask: to log a reading somewhere too small for
//  printf, to check a sensor value for NaN by hand, to meet a protocol that
//  ships raw IEEE 754 words. On this course's Cortex-M4F, `float` is THE
//  hardware float (the FPU is single-precision only), 32 bits laid out as:
//
//      bit  31       sign
//      bits 30..23   exponent, biased by 127
//      bits 22..0    mantissa (fraction)
//
//  The starter asks the question the FORBIDDEN way:
//
//      *(uint32_t *)&f
//
//  That is not "reading the bytes"; it is an access through an incompatible
//  pointer type -- an aliasing violation, undefined behaviour (C17 6.5 p7,
//  CERT EXP39-C). No warning fires, the value even comes out right at -O0,
//  and no sanitizer in this course catches it. That silence is the trap:
//  with optimisation on, the compiler may cache and reorder across the two
//  names for that memory, because you promised it they could not alias.
//
//  Two lawful spellings, each compiling to the same single instruction:
//
//      uint32_t bits;                        union {
//      memcpy(&bits, &f, sizeof bits);         uint32_t u;
//                                              float f;
//                                            } pun = {.f = f};  // read pun.u
//
//  memcpy also holds in C++; the union is C's own idiom. The reference
//  solution uses one of each, so you have seen both.
//
//  TASK
//    Rewrite `float_bits` and `float_from_bits` without the pointer casts,
//    and fix the field boundaries in `float_decompose` -- the starter's
//    shifts and masks are wrong too. Do not change the tests.
//
//  RUN IT
//    ./mec test 05_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(float) == sizeof(uint32_t), "IEEE 754 single expected");

struct float_parts {
  uint32_t sign;     // 0 or 1
  uint32_t exponent; // biased by 127: 127 means 2^0
  uint32_t mantissa; // the 23 fraction bits
};

uint32_t float_bits(float f) {
  // TODO: an aliasing violation (CERT EXP39-C). It "works" at -O0, which is
  // how it survives code review.
  return *(uint32_t *)&f;
}

float float_from_bits(uint32_t bits) {
  // TODO: the same violation, pointed the other way.
  return *(float *)&bits;
}

struct float_parts float_decompose(float f) {
  const uint32_t bits = float_bits(f);
  struct float_parts p;
  p.sign = bits >> 31;
  // TODO: the exponent is 8 bits starting at bit 23, the mantissa the 23
  // bits below it. These boundaries are off by one.
  p.exponent = (bits >> 22) & 0xFFu;
  p.mantissa = bits & 0x3FFFFFu;
  return p;
}

TEST("float_bits sees the IEEE 754 representation") {
  CHECK_EQ(float_bits(1.0f), 0x3F800000u);
  CHECK_EQ(float_bits(-2.0f), 0xC0000000u);
  CHECK_EQ(float_bits(0.0f), 0u);
}

TEST("decompose splits sign, exponent, mantissa") {
  const struct float_parts one = float_decompose(1.0f);
  CHECK_EQ(one.sign, 0u);
  CHECK_EQ(one.exponent, 127u); // 2^0, biased
  CHECK_EQ(one.mantissa, 0u);

  const struct float_parts half = float_decompose(0.5f);
  CHECK_EQ(half.exponent, 126u); // 2^-1

  const struct float_parts three_halves = float_decompose(1.5f);
  CHECK_EQ(three_halves.mantissa, 0x400000u); // .5 = top fraction bit

  const struct float_parts neg = float_decompose(-2.0f);
  CHECK_EQ(neg.sign, 1u);
  CHECK_EQ(neg.exponent, 128u); // 2^1
}

TEST("from_bits reconstructs a float") {
  CHECK_EQ(float_from_bits(0x3F800000u), 1.0f);
  CHECK_NEAR(float_from_bits(0x40490FDBu), 3.14159274f, 1e-6f); // pi
}
