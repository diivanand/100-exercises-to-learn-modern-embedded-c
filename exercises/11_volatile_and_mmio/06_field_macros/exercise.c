// =============================================================================
//  11.06 -- A vocabulary for register fields
// =============================================================================
//
//  By 11.03 you were writing `(reg & ~(3u << 10)) | (1u << 10)` by hand.
//  That is correct exactly once. The tenth time, someone shifts the mask
//  instead of the value, or reuses pin 5's shift for pin 6, and the review
//  cannot tell -- every line is a wall of shifts. Register-heavy code needs
//  a VOCABULARY, built once and tested once:
//
//      #define PWM_DUTY_MASK  0x00000FF0u      // the field's bits, in place
//      #define PWM_DUTY_POS   4u               // how far to shift
//
//      FIELD_GET(reg, PWM_DUTY)                // -> right-justified value
//      FIELD_SET(reg, PWM_DUTY, 0x55)          // -> new register value
//
//  This is exactly what vendor headers provide -- CMSIS calls the pair
//  _FLD2VAL and _VAL2FLD; ST's headers define *_Msk and *_Pos for every
//  field on the die. The ## paste (07.03) glues `PWM_DUTY` onto `_MASK`
//  and `_POS`, so a field's two halves cannot be mixed up across fields.
//  Macro hygiene rules from 07.01 apply in full: every parameter in
//  parentheses, each parameter evaluated once.
//
//  One policy question: what if the value does not fit the field --
//  FIELD_SET(reg, PWM_MODE, 0xFF) on a 3-bit field? Clip it with the mask,
//  ALWAYS: an oversized value must never smear into the neighbouring
//  fields (that turns one wrong parameter into three corrupted ones). And
//  in a debug build, assert too (09.05): clipping contains the caller's
//  bug, the assert reports it. The tests here check the containment.
//
//  TASK
//    Both macros below are wrong. FIELD_GET masks but forgets to shift
//    down, so values come back still sitting at their register position.
//    FIELD_SET forgets to shift the value UP (and clips it against the
//    in-place mask, which for any field above bit 0 throws the value
//    away). Fix both. Do not change the field definitions or the tests.
//
//  RUN IT
//    ./mec test 11_06
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

// --- field descriptions: a MASK/POS pair per field (given) --------------------

#define PWM_EN_MASK 0x00000001u
#define PWM_EN_POS 0u
#define PWM_MODE_MASK 0x0000000Eu
#define PWM_MODE_POS 1u
#define PWM_DUTY_MASK 0x00000FF0u
#define PWM_DUTY_POS 4u
#define PWM_PRESCALER_MASK 0xFFFF0000u
#define PWM_PRESCALER_POS 16u

// --- the vocabulary -------------------------------------------------------------

// TODO: masked, but never shifted down -- PWM_DUTY comes back as 0x230.
#define FIELD_GET(reg, F) ((reg) & F##_MASK)

// TODO: the value is never shifted up to the field's position.
#define FIELD_SET(reg, F, val) (((reg) & ~F##_MASK) | ((uint32_t)(val) & F##_MASK))

TEST("GET extracts right-justified values") {
  const uint32_t reg = 0xABCD1234u;
  CHECK_EQ(FIELD_GET(reg, PWM_EN), 0u);
  CHECK_EQ(FIELD_GET(reg, PWM_MODE), 2u);    // (0x1234 & 0xE) >> 1
  CHECK_EQ(FIELD_GET(reg, PWM_DUTY), 0x23u); // (0x1234 & 0xFF0) >> 4
  CHECK_EQ(FIELD_GET(reg, PWM_PRESCALER), 0xABCDu);
}

TEST("SET writes one field and preserves the rest") {
  const uint32_t reg = FIELD_SET(0xFFFFFFFFu, PWM_DUTY, 0x55u);
  CHECK_EQ(reg, 0xFFFFF55Fu);
  CHECK_EQ(FIELD_GET(reg, PWM_DUTY), 0x55u);
  CHECK_EQ(FIELD_GET(reg, PWM_PRESCALER), 0xFFFFu); // untouched
}

TEST("SET then GET round-trips") {
  uint32_t reg = 0;
  reg = FIELD_SET(reg, PWM_MODE, 5u);
  reg = FIELD_SET(reg, PWM_EN, 1u);
  CHECK_EQ(FIELD_GET(reg, PWM_MODE), 5u);
  CHECK_EQ(FIELD_GET(reg, PWM_EN), 1u);
}

TEST("an oversized value is clipped, never smeared into neighbours") {
  const uint32_t reg = FIELD_SET(0u, PWM_MODE, 0xFFu);
  CHECK_EQ(reg, PWM_MODE_MASK); // only the field's own bits
  CHECK_EQ(FIELD_GET(reg, PWM_EN), 0u);
  CHECK_EQ(FIELD_GET(reg, PWM_DUTY), 0u);
}
