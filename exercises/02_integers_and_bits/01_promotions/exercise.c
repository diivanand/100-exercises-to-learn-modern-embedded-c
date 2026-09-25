// =============================================================================
//  02.01 -- Integer promotions: there are no uint8_t in your expressions
// =============================================================================
//
//  The single most important fact about C arithmetic: OPERANDS SMALLER THAN
//  INT DO NOT STAY SMALL. Before almost any operator does its work, a
//  uint8_t or uint16_t operand is converted -- "promoted" -- to int. Your
//  expression full of uint8_t is computed in 32-bit signed arithmetic, and
//  only squeezed back down if you assign or cast it back.
//
//      uint8_t x = 0xFF;
//      ~x                    // NOT 0x00. ~(int)0xFF = 0xFFFFFF00 = -256.
//      (x << 8)              // an int, 0xFF00; x did not "shift out".
//      x + x                 // an int, 510; no wraparound at 255.
//
//  (Effective C ch. 3, "Integer Conversion Rank" and "Integer Promotions";
//  CERT INT02-C. Promotion preserves VALUE, not type: uint8_t fits in int,
//  so it goes to int -- signed -- not to unsigned int.)
//
//  Two real bugs below.
//
//  1. `is_all_ones` compares `~reg == 0`. Read the table above: for
//     reg = 0xFF, `~reg` is -256, not 0, and the comparison is false. The
//     fix is to force the width you meant: `(uint8_t)~reg == 0`, or compare
//     against 0xFF directly.
//
//  2. `frame_checksum_ok` implements a LIN-style additive checksum: the sum
//     of the payload bytes plus the checksum byte, TRUNCATED TO 8 BITS, must
//     be 0xFF. The starter forgets the truncation. Because the sum is
//     computed in int, the carries that should wrap at 8 bits pile up above
//     bit 7 instead, and any frame whose byte sum exceeds 0xFF is judged
//     corrupt. (This is the promotion trap in its natural habitat: checksum
//     code moved from an 8-bit AVR -- where char arithmetic wrapped "for
//     free" -- to a 32-bit part.)
//
//  3. `u32_from_be_bytes` assembles four bytes into a word, and there is a
//     third trap waiting in it: `b[0] << 24` promotes b[0] to INT -- signed
//     -- and for b[0] >= 0x80 the shift pushes a 1 into the sign bit, which
//     is undefined behaviour (02.04 dwells on shifts). It "works" today; the
//     UBSan build (cmake --preset asan) names the line. Promote by hand
//     BEFORE shifting: `(uint32_t)b[0] << 24`.
//
//  TASK
//    Fix all three functions. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// True if every bit of an 8-bit status register is set.
bool is_all_ones(uint8_t reg) {
  // TODO: ~reg is an int here, and it is never 0.
  return ~reg == 0;
}

// LIN-style checksum: (sum of payload bytes + checksum) mod 256 == 0xFF.
bool frame_checksum_ok(const uint8_t *payload, size_t len, uint8_t checksum) {
  unsigned sum = checksum;
  for (size_t i = 0; i < len; ++i) {
    sum += payload[i];
  }
  // TODO: the wrap at 8 bits never happens; sums above 0xFF always fail.
  return sum == 0xFF;
}

// Big-endian byte order: b[0] is the most significant byte.
uint32_t u32_from_be_bytes(const uint8_t b[4]) {
  // TODO: for b[0] >= 0x80 the first shift is UB (run the asan preset).
  return (uint32_t)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
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

  // 0xDE has the top bit set: without the cast, this one's first shift is
  // undefined behaviour. The value may still come out right at -O0 -- that
  // is what "undefined" means. The asan preset turns it into a report.
  const uint8_t top_bit[] = {0xDE, 0xAD, 0xBE, 0xEF};
  CHECK_EQ(u32_from_be_bytes(top_bit), 0xDEADBEEFu);
}
