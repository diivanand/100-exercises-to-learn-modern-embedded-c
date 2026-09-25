// =============================================================================
//  05.01 -- Struct layout: padding is real, and memcmp can see it
// =============================================================================
//
//  A struct's members are laid out IN DECLARATION ORDER (C17 6.7.2.1), the
//  first at offset zero. Between them the compiler inserts PADDING so each
//  member sits at an offset its type can live with -- on this course's
//  targets a uint32_t must start at a multiple of 4, a uint16_t at a
//  multiple of 2. After the last member comes TRAILING padding, rounding the
//  size up so that in an array of these structs the next element is aligned
//  too. (Effective C 2nd ed. ch. 2, "Alignment".)
//
//  The starter's record spends more on padding than it has to:
//
//      struct telemetry {          // offset  size  why
//        uint8_t flags;            //      0     1
//                                  //      1     3   pad (timestamp needs 4)
//        uint32_t timestamp;       //      4     4
//        uint8_t channel;          //      8     1
//                                  //      9     1   pad (reading needs 2)
//        uint16_t reading;         //     10     2
//        uint8_t mode;             //     12     1
//                                  //     13     3   pad (trailing, to 16)
//      };                          // sizeof == 16, for 9 bytes of data
//
//  Sort members LARGEST FIRST and the pads collapse: 4, 2, 1, 1, 1 packs
//  into 12 bytes with only trailing padding left. On a part with 128 KB of
//  RAM that is a third off every record in the log; a 4 KB flash page holds
//  341 twelve-byte records but only 256 sixteen-byte ones.
//  offsetof(struct telemetry, member) (<stddef.h>) prints the map, if you
//  want to watch the pads move while you reorder.
//
//  The second bug follows from the first: PADDING BYTES HOLD GARBAGE.
//  Nothing ever writes them, so two records can agree in every field and
//  still differ in the bytes memcmp reads. CERT EXP42-C: do not compare
//  padded structures with memcmp; compare member by member. (The
//  mirror-image rule is DCL39-C: never memcpy a padded struct across a trust
//  boundary either, because the pads leak whatever that RAM held before.
//  It bites kernels and bootloaders for real.)
//
//  When you need a layout with NO padding at all -- for a wire format or a
//  register map -- reordering is not the tool; explicit pack/unpack code is,
//  and that is the next exercise.
//
//  TASK
//    Reorder `struct telemetry` to fit the 12-byte budget, and rewrite
//    `telemetry_equal` to compare values rather than memory. Do not change
//    the tests.
//
//  RUN IT
//    ./mec test 05_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// TODO: 9 bytes of data are paying 7 bytes of padding. Reorder the members
// (the header comment has the map).
struct telemetry {
  uint8_t flags;
  uint32_t timestamp;
  uint8_t channel;
  uint16_t reading;
  uint8_t mode;
};

bool telemetry_equal(const struct telemetry *a, const struct telemetry *b) {
  // TODO: memcmp reads the padding bytes too, and nothing ever wrote them
  // (CERT EXP42-C).
  return memcmp(a, b, sizeof *a) == 0;
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
