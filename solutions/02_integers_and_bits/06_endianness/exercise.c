// Solution -- 02.06 Byte order is a property of storage, not of values

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

struct sensor_reading {
  uint16_t device_id;
  uint32_t timestamp;
  int16_t temp_centi; // hundredths of a degree, signed
};

static uint16_t read_be16(const uint8_t *p) {
  return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) |
         (uint32_t)p[3];
}

static int16_t as_i16(uint16_t u) {
  // The last portability wrinkle: converting a value above INT16_MAX to a
  // signed 16-bit type is IMPLEMENTATION-DEFINED in C17 (6.3.1.3p3) -- every
  // compiler you will meet wraps two's-complement, and C23 finally says so,
  // but this spelling is defined arithmetic in any C:
  if (u <= INT16_MAX) {
    return (int16_t)u;
  }
  return (int16_t)((int32_t)u - 65536);
}

// The wire format says: 8 bytes, big-endian, no padding anywhere --
//   [0..1] device id   [2..5] timestamp   [6..7] temperature
// So the parser reads BYTES at OFFSETS, exactly as the datasheet is written.
void parse_reading(const uint8_t frame[8], struct sensor_reading *out) {
  out->device_id = read_be16(frame + 0);
  out->timestamp = read_be32(frame + 2);
  out->temp_centi = as_i16(read_be16(frame + 6));
}

TEST("a frame parses field by field") {
  //                          id           timestamp               temp
  const uint8_t frame[8] = {0x12, 0x34, 0x00, 0x01, 0xE2, 0x40, 0x00, 0xFA};
  struct sensor_reading r;
  parse_reading(frame, &r);
  CHECK_EQ(r.device_id, 0x1234u);
  CHECK_EQ(r.timestamp, 123456u); // 0x0001E240
  CHECK_EQ(r.temp_centi, 250);    // +2.50 degrees
}

TEST("negative temperatures survive the trip") {
  const uint8_t frame[8] = {0xBE, 0xEF, 0x00, 0x00, 0x00, 0x01, 0xFF, 0x38};
  struct sensor_reading r;
  parse_reading(frame, &r);
  CHECK_EQ(r.device_id, 0xBEEFu);
  CHECK_EQ(r.timestamp, 1u);
  CHECK_EQ(r.temp_centi, -200); // 0xFF38: -2.00 degrees
}
