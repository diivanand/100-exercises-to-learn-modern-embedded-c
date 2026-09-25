// Solution -- 09.04 errno: the error channel that is only valid on failure

#include <mect/mect.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

bool parse_i32(const char *s, int32_t *out) {
  // The errno protocol, in full (CERT ERR30-C):
  //  1. clear it BEFORE the call -- success is not required to clear it,
  //     so whatever the last failure left there is still in the jar;
  //  2. use the RETURN VALUE and endptr to detect "no conversion";
  //  3. consult errno only to disambiguate what the return value cannot
  //     (LONG_MAX the value vs LONG_MAX the overflow clamp).
  errno = 0;
  char *end;
  const long value = strtol(s, &end, 10);

  if (end == s || *end != '\0') {
    return false; // nothing converted, or trailing junk ("42abc")
  }
  if (errno == ERANGE) {
    return false; // did not fit in long
  }
  if (value < INT32_MIN || value > INT32_MAX) {
    return false; // fit in long (64-bit here), but not in an int32_t
  }
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
  CHECK_FALSE(parse_i32("2147483648", &v));  // INT32_MAX + 1
  CHECK_FALSE(parse_i32("-2147483649", &v)); // INT32_MIN - 1
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
