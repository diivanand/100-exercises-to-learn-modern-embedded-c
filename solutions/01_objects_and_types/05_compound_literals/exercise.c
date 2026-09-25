// Solution -- 01.05 Compound literals: unnamed objects, and their lifetimes

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
  // Want a pointer that outlives the call? Then the object must have static
  // storage duration -- and the way to say so is to SAY SO. (A compound
  // literal at FILE scope would also have static storage; a named static
  // const is easier to find and to take seriously.)
  static const struct cal DEFAULT = {.gain = 2, .offset = 100};
  return &DEFAULT;
}

struct can_msg {
  uint32_t id;
  uint8_t len;
  uint8_t data[8];
};

void can_msg_reset(struct can_msg *m) {
  // Whole-object assignment from an all-zero value: every member, including
  // all eight payload bytes, in one line that cannot rot as the struct
  // grows. This is 01.03's reset idiom doing real work.
  *m = (struct can_msg){0};
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
