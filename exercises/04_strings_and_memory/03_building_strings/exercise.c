// =============================================================================
//  04.03 -- Building a line in a fixed buffer
// =============================================================================
//
//  Firmware assembles strings constantly -- log lines, command responses,
//  a display's worth of status -- and it does it into fixed buffers,
//  because that is all there is. The tool is snprintf, and the tool has a
//  contract worth knowing exactly (Effective C ch. 8):
//
//   - It NEVER writes more than `size` bytes, terminator included, and
//     (for size > 0) the result is always NUL-terminated.
//   - It returns the number of characters it WANTED to write, terminator
//     excluded. If that return is >= size, the output was TRUNCATED.
//   - It returns a NEGATIVE value on an encoding error. Rare, but a
//     negative number cast to size_t is a catastrophe, so the check is
//     not optional.
//
//  The "would have written" return is the trap when you chain calls:
//
//      offset += (size_t)snprintf(buf + offset, sizeof buf - offset, ...);
//
//  Once one call truncates, offset jumps PAST the end of the buffer. The
//  next call computes `sizeof buf - offset` -- unsigned arithmetic, so the
//  negative number you meant wraps to something astronomical (CERT
//  INT30-C) -- and happily writes through `buf + offset`, which points
//  outside the buffer. Two bugs deep, and the line between them is a
//  perfectly idiomatic-looking +=.
//
//  The fix is a commit step between snprintf and the bookkeeping: check
//  for negative, compare against the room that was actually there, and on
//  truncation CLAMP the length to capacity - 1 (which is where snprintf
//  really stopped) and remember that it happened. Wrap that once in a
//  helper and every log line in the code base inherits the discipline.
//
//  TASK
//    Fix `line_add_str` and `line_add_u32` (a shared helper is the clean
//    answer). After truncation the line must stay a valid C string, report
//    what happened, and refuse further pieces. Do not change the tests.
//
//  RUN IT
//    ./mec test 04_03
//
// =============================================================================

#include <mect/mect.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct line {
  char buf[32];
  size_t len;     // characters written, excluding the terminator
  bool truncated; // something did not fit (or failed to format)
};

void line_reset(struct line *l) {
  l->buf[0] = '\0';
  l->len = 0;
  l->truncated = false;
}

bool line_add_str(struct line *l, const char *s) {
  // TODO: n is what snprintf WANTED to write, not what it wrote; nothing
  // here notices truncation, and after it, len points past the buffer.
  int n = snprintf(l->buf + l->len, sizeof l->buf - l->len, "%s", s);
  l->len += (size_t)n;
  return true;
}

bool line_add_u32(struct line *l, uint32_t v) {
  // TODO: same two bugs. (PRIu32 rather than "%u" is deliberate -- on
  // arm-none-eabi, uint32_t is unsigned long and "%u" would be UB there.)
  int n = snprintf(l->buf + l->len, sizeof l->buf - l->len, "%" PRIu32, v);
  l->len += (size_t)n;
  return true;
}

TEST("a status line assembles in pieces") {
  struct line l;
  line_reset(&l);
  CHECK(line_add_str(&l, "temp="));
  CHECK(line_add_u32(&l, 231));
  CHECK(line_add_str(&l, " ok"));
  CHECK_EQ(l.buf, "temp=231 ok");
  CHECK_EQ(l.len, 11u);
  CHECK_FALSE(l.truncated);
}

TEST("truncation clamps instead of walking past the end") {
  struct line l;
  line_reset(&l);
  CHECK(line_add_str(&l, "sensor bank A: "));                    // 15 chars
  CHECK_FALSE(line_add_str(&l, "0123456789ABCDEF0123456789"));  // 26 more
  CHECK_EQ(l.len, 31u);         // capacity - 1, NOT 15 + 26
  CHECK(l.truncated);
  CHECK_EQ(strlen(l.buf), 31u); // still a valid, printable C string
  // A full line refuses further pieces rather than scribbling somewhere.
  CHECK_FALSE(line_add_u32(&l, 42));
  CHECK_EQ(strlen(l.buf), 31u);
}

TEST("the buffer is always a valid C string, even empty") {
  struct line l;
  line_reset(&l);
  CHECK_EQ(l.buf, "");
  CHECK_EQ(strlen(l.buf), 0u);
}
