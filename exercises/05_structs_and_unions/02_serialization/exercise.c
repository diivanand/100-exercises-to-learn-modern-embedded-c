// =============================================================================
//  05.02 -- Serialization: structs describe memory, not messages
// =============================================================================
//
//  Sooner or later firmware puts a struct ON A WIRE -- a UART frame, a radio
//  packet, a flash record. The tempting one-liner is memcpy: the bytes are
//  right there. But what those bytes ARE is this compiler's layout decisions
//  for this target: member order, the padding from 05.01, this machine's
//  byte order, this ABI's type sizes. That is not a message format; it is a
//  memory dump. The receiver only understands it if it runs the same code on
//  the same kind of chip -- which is exactly what the other end of a wire is
//  not.
//
//  A wire format is a CONTRACT, written down byte by byte, owned by no
//  compiler. This exercise's message:
//
//      offset  size  field
//           0     1  0x21 (message type: sensor report)
//           1     1  sensor id
//           2     2  reading, millivolts, big-endian
//           4     4  tick at capture, big-endian
//                 8  bytes total
//
//  The struct below and the wire above disagree in three ways at once: the
//  struct has a padding byte the wire must not carry (DCL39-C from 05.01
//  again -- that pad would leak one byte of RAM in every message); the wire
//  is big-endian while this course's targets store little-endian (Effective
//  C 2nd ed. ch. 8, "Endian"); and the wire must survive the struct being
//  reordered by someone doing 05.01 to it. So serialization is CODE, not a
//  cast: pack the fields one at a time with the shift idiom from 02.01.
//
//  (People reach for `#pragma pack` overlays here. That fixes only the
//  padding -- not the byte order, not the coupling -- at the price of
//  unaligned members and a vendor extension. Explicit packing costs a dozen
//  lines, once.)
//
//  One test below deserves a note: the ROUND TRIP passes even on the
//  starter, because the starter unpacks its own wrong bytes and the two
//  mistakes cancel. Round-trip tests prove consistency, not correctness; the
//  golden-byte tests are the ones that speak for the receiver. Keep both
//  habits for every protocol you touch.
//
//  TASK
//    Rewrite `report_pack` and `report_unpack` to speak the wire format in
//    the table above. Do not change the struct, and do not change the tests.
//
//  RUN IT
//    ./mec test 05_02
//
// =============================================================================

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

size_t report_pack(const struct sensor_report *r, uint8_t *out) {
  // TODO: this ships the struct's memory -- padding byte, little-endian
  // fields and all -- not the wire format in the header comment.
  out[0] = MSG_SENSOR_REPORT;
  memcpy(&out[1], r, sizeof *r);
  return 1 + sizeof *r;
}

bool report_unpack(const uint8_t *in, size_t len, struct sensor_report *out) {
  // TODO: the same mistake, pointed the other way.
  if (len < 1 + sizeof *out) {
    return false;
  }
  if (in[0] != MSG_SENSOR_REPORT) {
    return false;
  }
  memcpy(out, &in[1], sizeof *out);
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
  const struct sensor_report before = {
      .sensor_id = 9, .reading_mv = 1650, .tick = 4242};
  uint8_t wire[16] = {0};
  const size_t n = report_pack(&before, wire);
  struct sensor_report after;
  REQUIRE(report_unpack(wire, n, &after));
  CHECK_EQ(after.sensor_id, 9u);
  CHECK_EQ(after.reading_mv, 1650u);
  CHECK_EQ(after.tick, 4242u);
}
