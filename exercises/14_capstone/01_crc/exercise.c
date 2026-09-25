// =============================================================================
//  14.01 -- CRC-16: the checksum that earns its keep
// =============================================================================
//
//  Welcome to the capstone. Four exercises assemble one miniature firmware:
//  a "command station" that talks a binary protocol over a byte stream --
//  fed by tests here, and by a real UART when the bonus track (chapters
//  15-17) ports the same code onto the NUCLEO board. Almost nothing in this
//  chapter is new; the work is COMPOSITION. First layer: integrity.
//
//  The additive checksum from 02.01 misses reordered bytes, paired bit
//  flips, and every error that sums to zero. A CRC -- the remainder of
//  polynomial division over GF(2), in practice "shift and conditionally XOR
//  a constant" -- catches all single- and double-bit errors, odd numbers of
//  flips, and every burst shorter than the register. That is why it guards
//  CAN frames, Modbus, XMODEM, Ethernet, and flash images, usually in
//  exactly the two shapes below: a bitwise loop (small, slow, obviously
//  right) and a 256-entry table (a byte per step instead of a bit).
//
//  A WARNING FROM THE FIELD. "CRC-16" names a family, not an algorithm.
//  Polynomial, initial value, bit order ("reflection"), final XOR -- vary
//  any one and every output changes. Ours is CRC-16/CCITT-FALSE:
//
//      poly 0x1021, init 0xFFFF, MSB first, no reflection, no final XOR
//
//  and the standard way to prove an implementation is its CHECK VALUE: the
//  CRC of the ASCII string "123456789" must be 0x29B1. The starter's table
//  generator implements a DIFFERENT family member -- the reflected variant
//  (LSB-first, mirrored poly 0x8408, the one XMODEM does not use) -- which
//  is precisely the mistake people make copying CRC code off the internet.
//  The bitwise oracle above the table is correct; make the table match it.
//
//  Second bug: `crc16_update` "helpfully" resets the running value on every
//  call. That destroys the STREAMING contract -- update must continue from
//  wherever the caller's crc left off, because 14.02 will feed it one byte
//  at a time as they trickle out of a UART. Init is the CALLER's move
//  (that is what CRC16_INIT is for); update just folds bytes in.
//
//  TASK
//    Fix `crc16_init_tables` (MSB-first, matching the oracle) and
//    `crc16_update` (no reset). Do not change the tests.
//
//  RUN IT
//    ./mec test 14_01
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

#define CRC16_INIT 0xFFFFu
#define CRC16_POLY 0x1021u

// The oracle: the polynomial division written out bit by bit (given).
uint16_t crc16_bitwise(uint16_t crc, const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)((uint16_t)data[i] << 8);
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000u) {
        crc = (uint16_t)((uint16_t)(crc << 1) ^ CRC16_POLY);
      } else {
        crc = (uint16_t)(crc << 1);
      }
    }
  }
  return crc;
}

static uint16_t crc_table[256];

void crc16_init_tables(void) {
  // TODO: this is the REFLECTED variant -- a different CRC. Rewrite it to
  // do per-byte what the oracle does: byte into the TOP of the register,
  // test the TOP bit, shift LEFT, XOR 0x1021.
  for (uint32_t byte = 0; byte < 256u; ++byte) {
    uint16_t crc = (uint16_t)byte;
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 1u) {
        crc = (uint16_t)((uint16_t)(crc >> 1) ^ 0x8408u);
      } else {
        crc = (uint16_t)(crc >> 1);
      }
    }
    crc_table[byte] = crc;
  }
}

uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
  crc = CRC16_INIT; // TODO: this "reset" breaks hashing in chunks.
  for (size_t i = 0; i < len; ++i) {
    const uint8_t idx = (uint8_t)((crc >> 8) ^ data[i]);
    crc = (uint16_t)((uint16_t)(crc << 8) ^ crc_table[idx]);
  }
  return crc;
}

uint16_t crc16(const uint8_t *data, size_t len) {
  return crc16_update(CRC16_INIT, data, len);
}

TEST("the published check value: CRC of \"123456789\" is 0x29B1") {
  crc16_init_tables();
  const uint8_t *digits = (const uint8_t *)"123456789";
  CHECK_EQ(crc16_bitwise(CRC16_INIT, digits, 9), 0x29B1u);
  CHECK_EQ(crc16(digits, 9), 0x29B1u);
}

TEST("empty input leaves the initial value untouched") {
  crc16_init_tables();
  const uint8_t nothing[1] = {0};
  CHECK_EQ(crc16(nothing, 0), CRC16_INIT);
}

TEST("the table-driven version agrees with the bitwise oracle") {
  crc16_init_tables();
  uint8_t ramp[256];
  for (size_t i = 0; i < sizeof ramp; ++i) {
    ramp[i] = (uint8_t)i;
  }
  CHECK_EQ(crc16(ramp, sizeof ramp), crc16_bitwise(CRC16_INIT, ramp, sizeof ramp));

  const uint8_t frame[] = {0x02, 0x10, 0xDE, 0xAD};
  CHECK_EQ(crc16(frame, sizeof frame), 0x78A4u); // oracle-verified
  CHECK_EQ(crc16_bitwise(CRC16_INIT, frame, sizeof frame), 0x78A4u);

  const uint8_t one_high[] = {0x80};
  CHECK_EQ(crc16(one_high, 1), crc16_bitwise(CRC16_INIT, one_high, 1));
}

TEST("streaming: hashing in chunks equals hashing in one go") {
  crc16_init_tables();
  const uint8_t *digits = (const uint8_t *)"123456789";

  uint16_t crc = CRC16_INIT;
  crc = crc16_update(crc, digits, 2);
  crc = crc16_update(crc, digits + 2, 3);
  crc = crc16_update(crc, digits + 5, 4);
  CHECK_EQ(crc, 0x29B1u);

  crc = CRC16_INIT;
  for (size_t i = 0; i < 9; ++i) {
    crc = crc16_update(crc, digits + i, 1); // one byte at a time, like a UART
  }
  CHECK_EQ(crc, 0x29B1u);
}
