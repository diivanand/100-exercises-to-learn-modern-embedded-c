// Solution -- 06.01 Storage duration: whose memory is this?

#include <mect/mect.h>

#include <stdint.h>
#include <stdio.h>

const char *format_reading(char *out, size_t out_size, int centi_c) {
  // The caller owns the memory; this function only fills it. That is the
  // embedded idiom: no hidden static (one result alive at a time, and not
  // ISR-safe), no malloc (chapter 08), no dangling stack pointer. Returning
  // `out` is a courtesy so calls can nest inside a larger printf.
  int whole = centi_c / 10;
  int tenth = centi_c % 10;
  if (tenth < 0) {
    tenth = -tenth;
  }
  snprintf(out, out_size, "%d.%dC", whole, tenth);
  return out;
}

TEST("the result lives in the caller's buffer") {
  char out[16] = "";
  CHECK(format_reading(out, sizeof out, 42) == out);
  CHECK_EQ(out, "4.2C");
}

TEST("two readings can be alive at once") {
  char a[16] = "";
  char b[16] = "";
  const char *pa = format_reading(a, sizeof a, 235);
  const char *pb = format_reading(b, sizeof b, -40);
  CHECK_EQ(pa, "23.5C");
  CHECK_EQ(pb, "-4.0C");
}

TEST("zero and boundaries") {
  char out[16] = "";
  CHECK_EQ(format_reading(out, sizeof out, 0), "0.0C");
  CHECK_EQ(format_reading(out, sizeof out, 999), "99.9C");
}
