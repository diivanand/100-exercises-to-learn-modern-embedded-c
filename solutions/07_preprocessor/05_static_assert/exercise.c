// Solution -- 07.05 _Static_assert: make the compiler check the contract

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// The wire format, largest-first so no padding appears (05.01):
//   offset 0: timestamp, 4 bytes
//   offset 4: sensor_id, 2 bytes
//   offset 6: flags, 1 byte
//   offset 7: checksum, 1 byte
struct log_record {
  uint32_t timestamp;
  uint16_t sensor_id;
  uint8_t flags;
  uint8_t checksum;
};

// The contract, enforced at compile time. If a future edit disturbs the
// layout, the build breaks HERE, with these messages -- not in the field
// with a peer device rejecting records.
_Static_assert(sizeof(struct log_record) == 8,
               "log_record must be exactly 8 bytes on the wire");
_Static_assert(offsetof(struct log_record, timestamp) == 0,
               "timestamp must sit at offset 0");
_Static_assert(offsetof(struct log_record, sensor_id) == 4,
               "sensor_id must sit at offset 4");
_Static_assert(offsetof(struct log_record, flags) == 6,
               "flags must sit at offset 6");
_Static_assert(offsetof(struct log_record, checksum) == 7,
               "checksum must sit at offset 7");

// The RAM budget for the record buffer, also compile-time law.
#define RECORD_CAPACITY 64
_Static_assert(sizeof(struct log_record) * RECORD_CAPACITY <= 512,
               "record buffer exceeds its 512-byte RAM budget");

static struct log_record record_buffer[RECORD_CAPACITY];

// Per-kind scale factors. The trailing COUNT member exists so the table and
// the enum can be chained together by an assert.
enum sensor_kind { SENSOR_TEMP, SENSOR_PRESSURE, SENSOR_HUMIDITY, SENSOR_KIND_COUNT };

static const uint32_t sensor_scale[] = {10, 1, 2};

_Static_assert(sizeof sensor_scale / sizeof sensor_scale[0] == SENSOR_KIND_COUNT,
               "sensor_scale must have one entry per sensor_kind");

static uint32_t scale_for(enum sensor_kind kind) {
  return sensor_scale[kind];
}

TEST("the record layout matches the wire contract at run time too") {
  CHECK_EQ(sizeof(struct log_record), (size_t)8);
  CHECK_EQ(offsetof(struct log_record, checksum), (size_t)7);
  CHECK_EQ(sizeof record_buffer, (size_t)512);

  record_buffer[0] = (struct log_record){.timestamp = 1000, .sensor_id = 7};
  CHECK_EQ(record_buffer[0].timestamp, 1000u);
  CHECK_EQ(record_buffer[0].flags, 0u); // designated init zeroes the rest
}

TEST("every sensor kind has a scale") {
  CHECK_EQ(scale_for(SENSOR_TEMP), 10u);
  CHECK_EQ(scale_for(SENSOR_PRESSURE), 1u);
  CHECK_EQ(scale_for(SENSOR_HUMIDITY), 2u);
}
