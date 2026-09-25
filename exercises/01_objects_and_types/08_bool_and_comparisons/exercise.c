// =============================================================================
//  01.08 -- bool, truthiness, and the comparison that lies
// =============================================================================
//
//  C89 had no boolean type, so C grew a culture instead: zero is false,
//  everything else is true. C99 added _Bool and <stdbool.h> (bool, true,
//  false -- since C23 they are keywords, but include the header in C17).
//  The culture remains underneath, and the bugs live at the boundary
//  between the two. Three rules keep you out of them:
//
//   1. TEST BITS WITH `!= 0`, NEVER WITH `== 1`. A masked status bit is
//
//          status & STATUS_READY      // 0x04 or 0x00. Never 1.
//
//      `(status & STATUS_READY) == 1` compiles, reviews well if the
//      reviewer is tired, and is false forever -- unless the mask happens
//      to be bit 0, where it works and TEACHES someone to copy it. CERT
//      EXP46-C circles this whole family.
//
//   2. NORMALISE WHEN A 0-OR-1 IS PROMISED. Protocol fields and register
//      bits documented as "0 or 1" want `!!x` (or `x ? 1 : 0`): masking
//      alone hands the wire a 0x80.
//
//   3. CONVERSION TO bool IS `!= 0`, NOT TRUNCATION. (bool)0x100 is true;
//      (uint8_t)0x100 is 0. One is a question, the other a haircut. The
//      last test demonstrates the difference.
//
//  (Two neighbours of these bugs are caught by this build's flags already:
//  `if (x = 5)` and unparenthesised `x & MASK == MASK` -- precedence puts
//  the == first -- both trip -Wparentheses, which is in -Wall. The versions
//  below dodge the compiler by being well-formed and wrong, which is why
//  the tests exist.)
//
//  TASK
//    Fix all three functions. Do not change the tests.
//
//  RUN IT
//    ./mec test 01_08
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#define STATUS_HEATER 0x01u
#define STATUS_READY 0x04u
#define STATUS_ERROR 0x80u

bool link_ready(uint8_t status) {
  // TODO: the masked value is 0x04 when set. It is never 1.
  return (status & STATUS_READY) == 1;
}

bool heater_on(uint8_t status) {
  // TODO: same bug -- currently "working", because this mask is bit 0.
  return (status & STATUS_HEATER) == 1;
}

// The wire format documents this field as exactly 0 or 1.
uint8_t error_flag(uint8_t status) {
  // TODO: 0x80 is not 1.
  return (uint8_t)(status & STATUS_ERROR);
}

TEST("ready is a bit test, not an equality with 1") {
  CHECK(link_ready(0x04));
  CHECK(link_ready(0xFF));
  CHECK_FALSE(link_ready(0x00));
  CHECK_FALSE(link_ready(0xFB)); // everything BUT ready
}

TEST("the same pattern on bit 0, where the broken version got lucky") {
  CHECK(heater_on(0x01));
  CHECK(heater_on(0x05));
  CHECK_FALSE(heater_on(0xFE));
}

TEST("the error field is exactly 0 or 1 on the wire") {
  CHECK_EQ(error_flag(0x80), 1u);
  CHECK_EQ(error_flag(0xFF), 1u);
  CHECK_EQ(error_flag(0x7F), 0u);
}

TEST("bool conversion is a zero test, not a truncation") {
  // (uint8_t)0x100 is 0 -- narrowing keeps the low bits. (bool)0x100 is
  // true -- the conversion asks "is it nonzero?". Different questions.
  CHECK_EQ((uint8_t)0x100, 0u);
  CHECK_EQ((bool)0x100, true);
}
