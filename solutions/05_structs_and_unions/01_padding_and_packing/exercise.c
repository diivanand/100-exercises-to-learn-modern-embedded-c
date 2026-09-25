// Solution -- 05.01 Struct layout: padding is real, and memcmp can see it

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Largest first: 4, 2, 1, 1, 1. The only padding left is trailing (3 bytes,
// so an array element's timestamp stays 4-aligned): 12 bytes, down from 16.
// The data did not change; the order did.
struct telemetry {
  uint32_t timestamp; // offset 0
  uint16_t reading;   // offset 4
  uint8_t flags;      // offset 6
  uint8_t channel;    // offset 7
  uint8_t mode;       // offset 8, then 3 bytes trailing padding
}; // sizeof == 12

bool telemetry_equal(const struct telemetry *a, const struct telemetry *b) {
  // Member by member (CERT EXP42-C). The 3 trailing padding bytes are still
  // there and still garbage; a value comparison never reads them. memcmp
  // would be defensible only for a type PROVEN padding-free with a
  // _Static_assert on its size -- and it breaks again the day a member is
  // added.
  return a->timestamp == b->timestamp && a->reading == b->reading &&
         a->flags == b->flags && a->channel == b->channel && a->mode == b->mode;
}

TEST("a record fits its 12-byte flash slot") {
  CHECK_EQ(sizeof(struct telemetry), 12u);
}

TEST("records that agree in every field are equal") {
  // Same field values, different padding: one struct starts life as 0x00
  // bytes, the other as 0xFF. A value comparison must not care.
  struct telemetry a, b;
  memset(&a, 0x00, sizeof a);
  memset(&b, 0xFF, sizeof b);
  a.timestamp = 123456;
  a.reading = 512;
  a.flags = 0x03;
  a.channel = 7;
  a.mode = 1;
  b.timestamp = 123456;
  b.reading = 512;
  b.flags = 0x03;
  b.channel = 7;
  b.mode = 1;
  CHECK(telemetry_equal(&a, &b));
}

TEST("records that differ in a field are not equal") {
  struct telemetry a, b;
  memset(&a, 0x00, sizeof a);
  memset(&b, 0x00, sizeof b);
  a.timestamp = 100;
  b.timestamp = 100;
  a.reading = 512;
  b.reading = 513; // one count apart
  CHECK_FALSE(telemetry_equal(&a, &b));
}
