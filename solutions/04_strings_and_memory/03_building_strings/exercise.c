// Solution -- 04.03 Building a line in a fixed buffer

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

// The bookkeeping every snprintf chain needs, in one place. `n` is a raw
// snprintf return: negative on encoding error, otherwise the number of
// characters the call WANTED to write (terminator excluded).
static bool line_commit(struct line *l, int n) {
  if (n < 0) {
    l->truncated = true; // encoding error; the buffer content is unspecified
    l->buf[l->len] = '\0';
    return false;
  }
  const size_t wanted = (size_t)n;
  const size_t room = sizeof l->buf - l->len; // includes the NUL's slot
  if (wanted >= room) {
    // snprintf already stopped at room - 1 characters and terminated.
    // Clamp len to what is really there -- NOT to len + wanted.
    l->len = sizeof l->buf - 1;
    l->truncated = true;
    return false;
  }
  l->len += wanted;
  return true;
}

bool line_add_str(struct line *l, const char *s) {
  if (l->truncated) {
    return false; // the line is already full; keep it stable
  }
  return line_commit(l, snprintf(l->buf + l->len, sizeof l->buf - l->len, "%s", s));
}

bool line_add_u32(struct line *l, uint32_t v) {
  if (l->truncated) {
    return false;
  }
  // PRIu32, not "%u": on this Mac uint32_t is unsigned int, but on
  // arm-none-eabi it is unsigned LONG, and "%u" there is undefined
  // behaviour. inttypes.h spells the format portably.
  return line_commit(l, snprintf(l->buf + l->len, sizeof l->buf - l->len, "%" PRIu32, v));
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
  CHECK(line_add_str(&l, "sensor bank A: "));                  // 15 chars
  CHECK_FALSE(line_add_str(&l, "0123456789ABCDEF0123456789")); // 26 more
  CHECK_EQ(l.len, 31u); // capacity - 1, NOT 15 + 26
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
