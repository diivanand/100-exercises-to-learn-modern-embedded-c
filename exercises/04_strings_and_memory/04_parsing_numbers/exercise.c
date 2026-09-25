// =============================================================================
//  04.04 -- Parsing numbers that might be garbage
// =============================================================================
//
//  Every device with a UART grows a command line eventually, and every
//  command line parses numbers typed by something you do not control. The
//  function everyone learns first is exactly the wrong one:
//
//      int atoi(const char *s);
//
//  atoi has NO ERROR CHANNEL. atoi("0") is 0; atoi("garbage") is 0; you
//  cannot tell a zero from a failure. Trailing junk ("42abc") is silently
//  accepted. And if the value does not fit in int, the behaviour is
//  UNDEFINED -- not clamped, not wrapped: undefined. CERT ERR34-C exists
//  to say, in effect, "never use atoi on input you did not write
//  yourself"; CERT INT06-C names the replacement.
//
//  strtol is the grown-up version, and its whole value is in the parts
//  people skip (Effective C ch. 7):
//
//      errno = 0;                        // it only WRITES errno on failure
//      char *end;
//      long v = strtol(s, &end, 10);
//      end == s                          // nothing parsed at all
//      *end != '\0'                      // parsed, then hit junk
//      errno == ERANGE                   // did not fit in long (clamped)
//
//  Two footnotes for our world. First, RANGE: long is 32 bits on
//  arm-none-eabi and 64 on your Mac, so "fits in long" is not "fits in
//  int32_t" -- a portable parser checks the target range itself. Second,
//  ERRNO on a microcontroller: newlib keeps errno in its reentrancy
//  structure, so this works on bare metal too -- but strtol is exactly the
//  kind of stateful library call that has no business inside an interrupt
//  handler (12.06 returns to that).
//
//  The deliverable is the shape every config reader and command parser
//  wants: all-or-nothing, range-checked, junk-rejecting.
//
//  TASK
//    Rewrite `parse_i32` on top of strtol so the tests pass. Do not change
//    the tests.
//
//  RUN IT
//    ./mec test 04_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Parse a base-10 int32 from s. Strict: the WHOLE string must be one
// in-range number. On success, write it through out and return true.
bool parse_i32(const char *s, int32_t *out) {
  // TODO: no error channel, junk accepted, overflow undefined.
  *out = atoi(s);
  return true;
}

TEST("plain numbers parse") {
  int32_t v = -1;
  CHECK(parse_i32("42", &v));
  CHECK_EQ(v, 42);
  CHECK(parse_i32("-7", &v));
  CHECK_EQ(v, -7);
  CHECK(parse_i32("0", &v)); // "0" is a number, not an error
  CHECK_EQ(v, 0);
}

TEST("junk is rejected, not misread") {
  int32_t v = 0;
  CHECK_FALSE(parse_i32("42abc", &v));
  CHECK_FALSE(parse_i32("abc", &v));
  CHECK_FALSE(parse_i32("", &v));
  CHECK_FALSE(parse_i32("   ", &v));
}

TEST("out of range is rejected, not wrapped") {
  int32_t v = 0;
  CHECK_FALSE(parse_i32("99999999999", &v));
  CHECK_FALSE(parse_i32("-99999999999", &v));
  CHECK(parse_i32("2147483647", &v));
  CHECK_EQ(v, INT32_MAX);
  CHECK(parse_i32("-2147483648", &v));
  CHECK_EQ(v, INT32_MIN);
}
