// Solution -- 05.03 Unions and type punning: reading bits lawfully

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
  // The portable spelling: copy the object representation out. Compilers
  // turn this into a single register move -- there is no memcpy call in the
  // generated code -- and it is valid in C and C++ alike.
  uint32_t bits;
  memcpy(&bits, &f, sizeof bits);
  return bits;
}

float float_from_bits(uint32_t bits) {
  // The union spelling: write one member, read the other. C17 6.5.2.3
  // (footnote 107) blesses this reinterpretation in C -- C++ does not, which
  // is why memcpy above is the habit that travels. One of each here, so you
  // have seen both.
  const union {
    uint32_t u;
    float f;
  } pun = {.u = bits};
  return pun.f;
}

struct float_parts float_decompose(float f) {
  const uint32_t bits = float_bits(f);
  return (struct float_parts){
      .sign = bits >> 31,
      .exponent = (bits >> 23) & 0xFFu, // 8 bits, starting at bit 23
      .mantissa = bits & 0x7FFFFFu,     // the 23 bits below them
  };
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
