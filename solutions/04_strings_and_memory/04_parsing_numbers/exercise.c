// Solution -- 04.04 Parsing numbers that might be garbage

#include <mect/mect.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

bool parse_i32(const char *s, int32_t *out) {
  if (s == NULL || *s == '\0') {
    return false;
  }
  errno = 0; // strtol only SETS errno on failure; clear stale values first
  char *end = NULL;
  const long v = strtol(s, &end, 10);
  if (end == s) {
    return false; // no digits at all ("abc", "   ")
  }
  if (*end != '\0') {
    return false; // trailing junk ("42abc")
  }
  if (errno == ERANGE) {
    return false; // did not fit in long (strtol clamped it)
  }
  // long is 64 bits on this Mac, 32 on arm-none-eabi. The extra check is
  // what makes the function mean int32_t everywhere -- on a 64-bit long,
  // "99999999999" parses cleanly and ERANGE never fires.
  if (v < INT32_MIN || v > INT32_MAX) {
    return false;
  }
  *out = (int32_t)v;
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
