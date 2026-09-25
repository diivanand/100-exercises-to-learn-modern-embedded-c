// Solution -- 14.01 CRC-16: the checksum that earns its keep

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
  // One entry per possible top byte: the table row IS the bitwise loop's
  // work for that byte, done once. MSB-first, exactly like the oracle --
  // the reflected (LSB-first) variant is a different CRC family member and
  // produces different values for every input.
  for (uint32_t byte = 0; byte < 256u; ++byte) {
    uint16_t crc = (uint16_t)(byte << 8);
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000u) {
        crc = (uint16_t)((uint16_t)(crc << 1) ^ CRC16_POLY);
      } else {
        crc = (uint16_t)(crc << 1);
      }
    }
    crc_table[byte] = crc;
  }
}

uint16_t crc16_update(uint16_t crc, const uint8_t *data, size_t len) {
  // No reset in here: `crc` comes in carrying the state of everything
  // hashed so far. That is the whole streaming contract -- the frame
  // parser in 14.02 feeds this one byte at a time as they arrive.
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
