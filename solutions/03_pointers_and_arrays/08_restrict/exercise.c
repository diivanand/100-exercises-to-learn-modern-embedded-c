// Solution -- 03.08 restrict is a promise, and overlap needs memmove manners

#include <mect/mect.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// No restrict: this function's CONTRACT now allows overlap, so it must
// handle it -- which direction is safe depends on which side of the source
// the destination sits. This is precisely memmove's job description, and
// for plain bytes you should call memmove; this exercise builds the word
// version to make the reasoning visible.
void copy_words(uint32_t *dst, const uint32_t *src, size_t n) {
  if (dst == src || n == 0) {
    return;
  }
  // Ordering the two pointers with < is only defined for pointers into the
  // same array (C17 6.5.8p5). For the overlap question that is fine -- if
  // they are NOT in the same array they cannot overlap and either branch is
  // correct -- but going through uintptr_t makes the comparison defined
  // unconditionally, which is how libc implementations do it.
  if ((uintptr_t)dst < (uintptr_t)src) {
    for (size_t i = 0; i < n; ++i) {
      dst[i] = src[i]; // forward: reads stay ahead of writes
    }
  } else {
    for (size_t i = n; i-- > 0;) {
      dst[i] = src[i]; // backward: same argument, mirrored
    }
  }
  // (The `i-- > 0` shape is the unsigned count-DOWN idiom: test, then
  // decrement, and the loop body sees n-1 .. 0 with no underflow. 01.02.)
}

TEST("plain copy into a separate buffer") {
  static const uint32_t src[4] = {1, 2, 3, 4};
  uint32_t dst[4] = {0};
  copy_words(dst, src, 4);
  CHECK_MEM_EQ(dst, src, sizeof src);
}

TEST("shifting a payload right to make room for a header") {
  uint32_t buf[7] = {1, 2, 3, 4, 5, 0, 0};
  copy_words(buf + 2, buf, 5); // overlap, dst above src
  static const uint32_t want[7] = {1, 2, 1, 2, 3, 4, 5};
  CHECK_MEM_EQ(buf, want, sizeof want);
}

TEST("compacting left still works") {
  uint32_t buf[6] = {9, 9, 7, 8, 9, 10};
  copy_words(buf, buf + 2, 4); // overlap, dst below src: forward is right
  CHECK_EQ(buf[0], 7u);
  CHECK_EQ(buf[3], 10u);
}
