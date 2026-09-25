// Solution -- 01.02 size_t, and how to count down with it

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

ptrdiff_t find_last(const uint8_t *data, size_t len, uint8_t needle) {
  // The goes-down-to idiom: test, THEN decrement. The body sees len-1 first
  // and 0 last, and when len == 0 the condition fails before any index is
  // formed. No underflow, no special case.
  for (size_t i = len; i-- > 0;) {
    if (data[i] == needle) {
      return (ptrdiff_t)i;
    }
  }
  return -1;
}

size_t count_trailing_padding(const uint8_t *data, size_t len, uint8_t pad) {
  size_t count = 0;
  for (size_t i = len; i-- > 0;) {
    if (data[i] != pad) {
      break;
    }
    ++count;
  }
  return count;
}

TEST("find_last prefers the last occurrence") {
  const uint8_t buf[] = {5, 9, 5, 7};
  CHECK_EQ(find_last(buf, sizeof buf, 5), 2);
}

TEST("find_last reaches index 0") {
  const uint8_t buf[] = {9, 1, 2, 3};
  CHECK_EQ(find_last(buf, sizeof buf, 9), 0);
}

TEST("find_last reports absence, including in an empty buffer") {
  const uint8_t buf[] = {1, 2, 3};
  CHECK_EQ(find_last(buf, sizeof buf, 9), -1);
  CHECK_EQ(find_last(buf, 0, 1), -1);
}

TEST("trailing padding is counted, and the count stops at the data") {
  const uint8_t frame[] = {0x10, 0x2A, 0xFF, 0xFF};
  CHECK_EQ(count_trailing_padding(frame, sizeof frame, 0xFF), 2u);
  CHECK_EQ(count_trailing_padding(frame, sizeof frame, 0x00), 0u);
}

TEST("an all-padding buffer is counted exactly once") {
  // The guard byte sits immediately BEFORE the buffer. A scan that runs off
  // the front finds more 0xFF there and keeps going -- the count comes back
  // bigger than the buffer. That is the underflow, made visible.
  struct {
    uint8_t guard;
    uint8_t buf[4];
  } t = {0xFF, {0xFF, 0xFF, 0xFF, 0xFF}};
  CHECK_EQ(count_trailing_padding(t.buf, sizeof t.buf, 0xFF), 4u);
}
