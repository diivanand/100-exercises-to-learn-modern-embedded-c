// =============================================================================
//  01.06 -- Enums: named states, wire values, and the switch
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR -- a switch that no longer
//  covers its enum. That diagnostic is half the lesson.
//
//  An enum gives related constants a shared, named type:
//
//      enum wake_source { WAKE_RESET = 0x01, WAKE_TIMER = 0x02, ... };
//
//  Three facts worth engraving (Effective C ch. 2, "Tags"):
//
//   1. WRITE EXPLICIT VALUES WHEN VALUES ESCAPE. If enumerators go into
//      logs, registers or packets, their numbers are a contract with other
//      code and other decades. Spell them out; never let "whatever order
//      they were listed in" define a wire format.
//
//   2. AN ENUM SWITCH WITHOUT `default` IS A TRIPWIRE -- a good one. With
//      -Wswitch (in -Wall), omitting an enumerator from such a switch is a
//      diagnostic, and with -Werror it stops the build. Someone added
//      WAKE_WATCHDOG below during a brown-out investigation; the compiler
//      is currently pointing at every switch that has not caught up. Adding
//      `default:` to an enum switch buys silence today at the price of that
//      tripwire for every future enumerator. (MISRA C:2012 16.4 demands a
//      default; this course sides with the tripwire for enum switches, and
//      13.01 revisits the trade.)
//
//   3. AN ENUM VARIABLE IS JUST AN INTEGER. In C17 the enumerators
//      themselves have type int, the variable an implementation-chosen
//      integer type (C23 adds control over it; nothing you can use here).
//      Nothing stops `(enum wake_source)0xFF` -- so bytes arriving off the
//      wire are validated at the boundary, or they are not validated at
//      all. Do not overlay an enum on a hardware register or a packet byte
//      either: its size is the implementation's choice, not yours.
//
//  `wake_source_from_wire` below currently "validates" with a cast --
//  point 3, done wrong. Its tests check that garbage bytes are refused and
//  that a refused byte leaves the output untouched.
//
//  TASK
//    Give WAKE_WATCHDOG its case, then make from_wire actually validate.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 01_06
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// Explicit values: these appear in flash logs read by tools that outlive any
// one firmware build.
enum wake_source {
  WAKE_RESET = 0x01,
  WAKE_TIMER = 0x02,
  WAKE_PIN = 0x04,
  WAKE_WATCHDOG = 0x08, // added for the brown-out investigation
};

uint8_t wake_source_to_wire(enum wake_source s) {
  // TODO: the enum has grown; the compiler is telling you exactly where.
  switch (s) {
  case WAKE_RESET:
    return 0x01;
  case WAKE_TIMER:
    return 0x02;
  case WAKE_PIN:
    return 0x04;
  }
  return 0;
}

bool wake_source_from_wire(uint8_t byte, enum wake_source *out) {
  // TODO: a cast is not validation. Any byte "succeeds" here.
  *out = (enum wake_source)byte;
  return true;
}

TEST("every wake source has a wire value") {
  CHECK_EQ(wake_source_to_wire(WAKE_RESET), 0x01u);
  CHECK_EQ(wake_source_to_wire(WAKE_TIMER), 0x02u);
  CHECK_EQ(wake_source_to_wire(WAKE_PIN), 0x04u);
  CHECK_EQ(wake_source_to_wire(WAKE_WATCHDOG), 0x08u);
}

TEST("decoding accepts exactly the published values") {
  enum wake_source s;
  CHECK(wake_source_from_wire(0x02, &s));
  CHECK_EQ(s, WAKE_TIMER);
  CHECK(wake_source_from_wire(0x08, &s));
  CHECK_EQ(s, WAKE_WATCHDOG);
}

TEST("decoding rejects everything else") {
  enum wake_source s = WAKE_RESET;
  CHECK_FALSE(wake_source_from_wire(0x00, &s));
  CHECK_FALSE(wake_source_from_wire(0x03, &s)); // two bits at once
  CHECK_FALSE(wake_source_from_wire(0xFF, &s));
  CHECK_EQ(s, WAKE_RESET); // a rejected byte must not modify the output
}
