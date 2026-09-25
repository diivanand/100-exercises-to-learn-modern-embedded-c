// Solution -- 02.01 Integer promotions: there are no uint8_t in your expressions

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool is_all_ones(uint8_t reg) {
  // Truncate the complement back to the width we meant. `reg == 0xFF` is
  // just as correct; this form survives a change of register width better
  // (make it a typedef and the 0xFF would silently need to become 0xFFFF).
  return (uint8_t)~reg == 0;
}

bool frame_checksum_ok(const uint8_t *payload, size_t len, uint8_t checksum) {
  unsigned sum = checksum;
  for (size_t i = 0; i < len; ++i) {
    sum += payload[i];
  }
  // The truncation IS the algorithm: an additive checksum is defined mod
  // 256. Masking with 0xFFu says so; casting to uint8_t says the same.
  return (sum & 0xFFu) == 0xFF;
}

uint32_t u32_from_be_bytes(const uint8_t b[4]) {
  // Promote by hand, to the UNSIGNED type, before the shift happens. Each
  // operand of | is then already uint32_t and no intermediate is ever
  // signed. This function is the portable answer to "how do I read a
  // big-endian field?" -- and 02.06 will show why a cast-the-pointer
  // "shortcut" is not.
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) |
         (uint32_t)b[3];
}

TEST("is_all_ones") {
  CHECK(is_all_ones(0xFF));
  CHECK_FALSE(is_all_ones(0xFE));
  CHECK_FALSE(is_all_ones(0x00));
}

TEST("checksum accepts valid frames, including ones with carries") {
  // 0x01 + 0x02 + checksum 0xFC = 0xFF: no carry, even the starter agrees.
  const uint8_t easy[] = {0x01, 0x02};
  CHECK(frame_checksum_ok(easy, sizeof easy, 0xFC));

  // 0xF0 + 0x20 + 0x30 = 0x140; + checksum 0xBF = 0x1FF; low byte 0xFF.
  // The sum carried past 8 bits -- exactly what the truncation is for.
  const uint8_t carry[] = {0xF0, 0x20, 0x30};
  CHECK(frame_checksum_ok(carry, sizeof carry, 0xBF));
}

TEST("checksum rejects corrupt frames") {
  const uint8_t frame[] = {0xF0, 0x20, 0x30};
  CHECK_FALSE(frame_checksum_ok(frame, sizeof frame, 0xBE)); // off by one
  CHECK_FALSE(frame_checksum_ok(frame, sizeof frame, 0x00));
}

TEST("bytes assemble big-endian, top bit set or not") {
  const uint8_t plain[] = {0x12, 0x34, 0x56, 0x78};
  CHECK_EQ(u32_from_be_bytes(plain), 0x12345678u);

  const uint8_t top_bit[] = {0xDE, 0xAD, 0xBE, 0xEF};
  CHECK_EQ(u32_from_be_bytes(top_bit), 0xDEADBEEFu);
}
