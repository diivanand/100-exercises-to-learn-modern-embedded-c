// Solution -- 05.02 Serialization: structs describe memory, not messages

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
  MSG_SENSOR_REPORT = 0x21,
  REPORT_WIRE_SIZE = 8,
};

struct sensor_report {
  uint8_t sensor_id;
  uint16_t reading_mv;
  uint32_t tick;
};

// The shift idiom from 02.01, pointed at output. Every byte of the wire is
// written by name; no padding, no native byte order, no ABI can sneak in.
static void put_be16(uint8_t *out, uint16_t v) {
  out[0] = (uint8_t)(v >> 8);
  out[1] = (uint8_t)v;
}

static void put_be32(uint8_t *out, uint32_t v) {
  out[0] = (uint8_t)(v >> 24);
  out[1] = (uint8_t)(v >> 16);
  out[2] = (uint8_t)(v >> 8);
  out[3] = (uint8_t)v;
}

static uint16_t get_be16(const uint8_t *in) {
  return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

static uint32_t get_be32(const uint8_t *in) {
  return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) |
         (uint32_t)in[3];
}

size_t report_pack(const struct sensor_report *r, uint8_t *out) {
  out[0] = MSG_SENSOR_REPORT;
  out[1] = r->sensor_id;
  put_be16(&out[2], r->reading_mv);
  put_be32(&out[4], r->tick);
  return REPORT_WIRE_SIZE;
}

bool report_unpack(const uint8_t *in, size_t len, struct sensor_report *out) {
  // Exactly the wire size, exactly the expected type byte. A parser that
  // accepts "roughly right" frames is a parser that will one day accept a
  // frame meant for someone else (14.02 builds the full framing).
  if (len != REPORT_WIRE_SIZE || in[0] != MSG_SENSOR_REPORT) {
    return false;
  }
  out->sensor_id = in[1];
  out->reading_mv = get_be16(&in[2]);
  out->tick = get_be32(&in[4]);
  return true;
}

TEST("pack emits the wire format, byte for byte") {
  const struct sensor_report r = {
      .sensor_id = 0x07, .reading_mv = 0x1234, .tick = 0xDEADBEEF};
  uint8_t wire[16] = {0};
  CHECK_EQ(report_pack(&r, wire), 8u);
  const uint8_t expected[8] = {0x21, 0x07, 0x12, 0x34, 0xDE, 0xAD, 0xBE, 0xEF};
  CHECK_MEM_EQ(wire, expected, sizeof expected);
}

TEST("unpack reads a frame the wire way") {
  const uint8_t wire[8] = {0x21, 0x03, 0x0B, 0xB8, 0x00, 0x00, 0x30, 0x39};
  struct sensor_report r;
  REQUIRE(report_unpack(wire, sizeof wire, &r));
  CHECK_EQ(r.sensor_id, 0x03u);
  CHECK_EQ(r.reading_mv, 3000u); // 0x0BB8
  CHECK_EQ(r.tick, 12345u);      // 0x3039
}

TEST("unpack rejects frames that are not this message") {
  struct sensor_report r;
  const uint8_t truncated[4] = {0x21, 0x03, 0x0B, 0xB8};
  CHECK_FALSE(report_unpack(truncated, sizeof truncated, &r));
  const uint8_t wrong_type[8] = {0x22, 0x03, 0x0B, 0xB8, 0x00, 0x00, 0x30, 0x39};
  CHECK_FALSE(report_unpack(wrong_type, sizeof wrong_type, &r));
}

TEST("a round trip proves consistency, not correctness") {
  // The starter passes this one: its pack and unpack share the same wrong
  // idea of the bytes, and the mistakes cancel. The golden-byte tests above
  // are the ones that speak for the receiver.
  const struct sensor_report before = {.sensor_id = 9, .reading_mv = 1650, .tick = 4242};
  uint8_t wire[16] = {0};
  const size_t n = report_pack(&before, wire);
  struct sensor_report after;
  REQUIRE(report_unpack(wire, n, &after));
  CHECK_EQ(after.sensor_id, 9u);
  CHECK_EQ(after.reading_mv, 1650u);
  CHECK_EQ(after.tick, 4242u);
}
