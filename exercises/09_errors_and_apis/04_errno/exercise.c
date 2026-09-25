// =============================================================================
//  09.04 -- errno: the error channel that is only valid on failure
// =============================================================================
//
//  The C library predates the advice in 09.01, and its error channel shows
//  its age: `errno` is a global-ish int that library calls SET on failure
//  and -- this is the part that bites -- NEVER CLEAR ON SUCCESS. It is not
//  a status register you can read at will; it is a note pinned to a
//  failure, meaningful only in the moment one was reported.
//
//  The protocol, and it is strict (CERT ERR30-C):
//
//   1. set errno to 0 yourself, immediately before the call;
//   2. detect failure from the RETURN VALUE (and, for strtol, endptr);
//   3. only then read errno, to learn WHICH failure -- e.g. strtol returns
//      LONG_MAX both for the genuine value and for overflow, and only
//      errno == ERANGE tells them apart.
//
//  Skip step 1 and yesterday's failure convicts today's success. That is
//  the starter's bug, and the last test stages exactly that prosecution:
//  parse something huge (fails, leaves ERANGE), then parse "42" -- which
//  the starter rejects, because it mistakes the stale ERANGE for its own.
//
//  The starter also trusts a cast instead of a range check: on this
//  machine `long` is 64 bits, so "2147483648" converts without ERANGE and
//  then silently truncates into the int32_t. Effective C ch. 7 walks
//  through strtol's contract; CERT ERR34-C says "use strtol, not atoi" --
//  this exercise is the reason the advice has teeth.
//
//  WHY THIS MATTERS OFF THE DESKTOP. errno is the textbook example of why
//  new APIs should not hide state behind the call (09.01 returned it all).
//  A hidden global is hostile to interrupts and threads; POSIX systems
//  paper over half of it by making errno thread-local, and newlib (the
//  embedded libc, chapters 15+) keeps one per reentrancy context for the
//  same reason. The design is grandfathered in; learn its handshake,
//  don't copy it.
//
//  TASK
//    Fix `parse_i32`: clear errno first, and reject values an int32_t
//    cannot hold instead of truncating them. Do not change the tests.
//
//  RUN IT
//    ./mec test 09_04
//
// =============================================================================

#include <mect/mect.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

bool parse_i32(const char *s, int32_t *out) {
  char *end;
  const long value = strtol(s, &end, 10);

  if (end == s || *end != '\0') {
    return false; // nothing converted, or trailing junk ("42abc")
  }
  // TODO: whose ERANGE is this? Nothing cleared errno before the call.
  if (errno != 0) {
    return false;
  }
  // TODO: on a 64-bit host this cast quietly eats "2147483648".
  *out = (int32_t)value;
  return true;
}

TEST("plain numbers parse") {
  int32_t v = 0;
  CHECK(parse_i32("42", &v));
  CHECK_EQ(v, 42);
  CHECK(parse_i32("-7", &v));
  CHECK_EQ(v, -7);
  CHECK(parse_i32("0", &v));
  CHECK_EQ(v, 0);
}

TEST("garbage and trailing junk are rejected") {
  int32_t v = 99;
  CHECK_FALSE(parse_i32("", &v));
  CHECK_FALSE(parse_i32("abc", &v));
  CHECK_FALSE(parse_i32("42abc", &v));
  CHECK_EQ(v, 99); // and the out-parameter kept its default (09.02)
}

TEST("values that do not fit an int32_t are rejected, not truncated") {
  int32_t v = 0;
  CHECK_FALSE(parse_i32("2147483648", &v));            // INT32_MAX + 1
  CHECK_FALSE(parse_i32("-2147483649", &v));           // INT32_MIN - 1
  CHECK_FALSE(parse_i32("999999999999999999999", &v)); // > LONG_MAX too
}

TEST("a stale ERANGE must not poison the next parse") {
  int32_t v = 0;
  // This parse fails and leaves ERANGE in errno -- which is allowed to
  // STAY there. Nothing clears errno on success.
  CHECK_FALSE(parse_i32("999999999999999999999", &v));
  // A parser that reads errno without clearing it first now sees the old
  // failure and rejects a perfectly good "42".
  CHECK(parse_i32("42", &v));
  CHECK_EQ(v, 42);
}
