// Solution -- 00.02 Take the compiler seriously

#include <mect/mect.h>

#include <stdint.h>

// The prototype: now the compiler knows the signature at the call site. In a
// real project this line would live in a header; in a single-file exercise it
// lives here. (Reordering the definitions works too -- many embedded code
// bases simply define bottom-up and keep helpers first.)
static uint16_t read_be16(const uint8_t *b);

uint16_t message_id(const uint8_t *frame) {
  return read_be16(frame);
}

static uint16_t read_be16(const uint8_t *b) {
  return (uint16_t)((uint16_t)(b[0] << 8) | b[1]);
}

uint8_t battery_percent(uint8_t raw) {
  // Integer arithmetic, multiply before divide (00.01). The product is at
  // most 255 * 100 = 25,500, so do it in unsigned int and narrow at the end
  // -- explicitly, with a cast that says "I have checked this fits".
  return (uint8_t)(raw * 100u / 255u);
}

uint8_t pack_status(uint8_t percent, uint8_t link_ok) {
  // The unused-parameter warning was the bug report. Fold the link bit in.
  // (The cast is needed because | promotes both operands to int -- 02.01
  // is all about that.)
  return (uint8_t)((link_ok ? 0x80u : 0u) | percent);
}

TEST("message id is read big-endian") {
  const uint8_t frame[] = {0x12, 0x34, 0xFF, 0xFF};
  CHECK_EQ(message_id(frame), 0x1234u);
}

TEST("battery percent scales 0..255 to 0..100") {
  CHECK_EQ(battery_percent(255), 100u);
  CHECK_EQ(battery_percent(128), 50u);
  CHECK_EQ(battery_percent(0), 0u);
}

TEST("status byte carries the link bit AND the percent") {
  CHECK_EQ(pack_status(100, 1), 0xE4u); // 0x80 | 100
  CHECK_EQ(pack_status(50, 0), 0x32u);
  CHECK_EQ(pack_status(0, 1), 0x80u);
}
