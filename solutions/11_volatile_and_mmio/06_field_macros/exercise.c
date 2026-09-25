// Solution -- 11.06 A vocabulary for register fields

#include <mect/mect.h>

#include <stdint.h>

// --- field descriptions: a MASK/POS pair per field (given) --------------------
// The ## in the macros below pastes the field name onto _MASK and _POS, so
// the two halves of a description can never be mixed across fields -- the
// 07.03 trick doing real work.

#define PWM_EN_MASK 0x00000001u
#define PWM_EN_POS 0u
#define PWM_MODE_MASK 0x0000000Eu
#define PWM_MODE_POS 1u
#define PWM_DUTY_MASK 0x00000FF0u
#define PWM_DUTY_POS 4u
#define PWM_PRESCALER_MASK 0xFFFF0000u
#define PWM_PRESCALER_POS 16u

// --- the vocabulary -------------------------------------------------------------

// Mask first, then shift down: the field arrives right-justified.
#define FIELD_GET(reg, F) (((reg) & F##_MASK) >> F##_POS)

// Shift the VALUE up to the field's position, and clip it to the mask so an
// oversized value can never spill into a neighbouring field. Clipping is
// the defensive half of the policy; the other half is an assert in debug
// builds (09.05) so the caller's bug is heard, not just contained.
#define FIELD_SET(reg, F, val)                                                           \
  (((reg) & ~F##_MASK) | (((uint32_t)(val) << F##_POS) & F##_MASK))

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
