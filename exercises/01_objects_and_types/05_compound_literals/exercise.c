// =============================================================================
//  01.05 -- Compound literals: unnamed objects, and their lifetimes
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR. The compiler has caught
//  one of the two bugs for you; read its message before reading on.
//
//  A compound literal creates an object with no name, right in an
//  expression (C99; C17 6.5.2.5):
//
//      apply_cal((struct cal){.gain = 3, .offset = -12}, raw);
//      send_bytes((const uint8_t[]){0xAA, 0x55}, 2);
//
//  It pairs beautifully with designated initialisers (01.04), and it powers
//  the reset idiom from 01.03:
//
//      *msg = (struct can_msg){0};      // every member zeroed, forever
//
//  What it does NOT create is a long-lived object. The rule (6.5.2.5p15):
//  inside a function, a compound literal has AUTOMATIC storage associated
//  with the enclosing BLOCK. Take its address and keep it past that block --
//  past the closing brace, past the return -- and you hold a pointer to a
//  corpse. At file scope the same syntax makes a static object instead,
//  which is consistent once you see it: the literal lives exactly as long
//  as anything else declared where it stands.
//
//  Below, `default_cal` returns the address of a compound literal. Clang's
//  -Wreturn-stack-address sees through it, and this course builds with
//  warnings as errors, so the bug is a diagnostic instead of a field return.
//  Do not fix it by caching the pointer somewhere the compiler cannot see --
//  fix the LIFETIME: the object wants to be `static const`.
//
//  The second bug is quieter: `can_msg_reset` clears the message header
//  fields one by one and forgets the payload, so eight stale bytes ride
//  along into the next transmission. The reset idiom above fixes it in one
//  line.
//
//  TASK
//    Fix default_cal's lifetime and can_msg_reset's completeness. Do not
//    change the tests.
//
//  RUN IT
//    ./mec test 01_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

struct cal {
  int32_t gain;
  int32_t offset;
};

int32_t apply_cal(struct cal c, int32_t raw) {
  return raw * c.gain + c.offset;
}

const struct cal *default_cal(void) {
  // TODO: this literal's storage ends at the closing brace below.
  return &(struct cal){.gain = 2, .offset = 100};
}

struct can_msg {
  uint32_t id;
  uint8_t len;
  uint8_t data[8];
};

void can_msg_reset(struct can_msg *m) {
  // TODO: the payload is not part of this reset.
  m->id = 0;
  m->len = 0;
}

TEST("a compound literal is a first-class argument") {
  // No named temporary, no file-scope clutter: the calibration exists for
  // exactly this call.
  CHECK_EQ(apply_cal((struct cal){.gain = 3, .offset = -12}, 100), 288);
  CHECK_EQ(apply_cal((struct cal){.gain = 1}, 55), 55); // offset zeroed
}

TEST("the default calibration outlives the function that hands it out") {
  const struct cal *c = default_cal();
  CHECK_EQ(c->gain, 2);
  CHECK_EQ(c->offset, 100);
  CHECK_EQ(apply_cal(*c, 50), 200);
}

TEST("reset clears the payload, not just the header fields") {
  struct can_msg m = {.id = 0x123, .len = 8};
  for (uint8_t i = 0; i < 8; ++i) {
    m.data[i] = 0xEE;
  }

  can_msg_reset(&m);

  CHECK_EQ(m.id, 0u);
  CHECK_EQ(m.len, 0u);
  for (uint8_t i = 0; i < 8; ++i) {
    CHECK_EQ(m.data[i], 0u); // stale payload bytes must not leak out later
  }
}
