// =============================================================================
//  03.08 -- restrict is a promise, and overlap needs memmove manners
// =============================================================================
//
//  `restrict` (C99) is a one-word contract on a pointer parameter: FOR THE
//  DURATION OF THIS CALL, NOTHING ELSE ACCESSES THIS OBJECT. Given that
//  promise the optimiser can keep values in registers instead of reloading
//  them after every store, vectorise loops, reorder freely. It is why these
//  two prototypes differ by more than documentation:
//
//      void *memcpy (void *restrict dst, const void *restrict src, size_t n);
//      void *memmove(void *dst, const void *src, size_t n);
//
//  memcpy DEMANDS no overlap and is fast because it may assume it; memmove
//  promises nothing and pays for it with a direction check. Every pointer
//  parameter in <string.h> has carried these qualifiers since C99, so you
//  have been reading restrict all along. In embedded code you will write it
//  on leaf math kernels and DMA staging helpers -- places where you control
//  every caller and the loop is hot.
//
//  The catch: restrict is a promise the compiler CANNOT CHECK. Call a
//  restrict-qualified function with overlapping pointers and you get no
//  warning, no error -- just undefined behaviour (CERT EXP43-C). And unlike
//  most UB in this course, there is nothing to fix at the call site: if
//  callers legitimately need overlap, the CONTRACT is what is wrong.
//
//  THE PROBLEM AT HAND. `copy_words` was written as a word-wise memcpy,
//  restrict and all. Then a protocol layer started using it to shift a
//  payload two slots up the SAME buffer, making room for a header -- the
//  second test. dst overlaps src, the promise is broken, and the forward
//  loop overwrites source words it has not read yet: watch the expected
//  values in that test -- 1, 2 copied over 3, 4 before they are read. (At
//  -O0 the corruption is at least deterministic. With optimisation and a
//  vectoriser acting on the broken promise, it need not be.)
//
//  The fix is to change the contract: drop restrict, and copy in whichever
//  DIRECTION is safe -- forward when dst sits below src, backward when it
//  sits above. That is what memmove has always done, and for bytes you
//  should simply call memmove; here you write the word version so the
//  direction argument is yours, not folklore.
//
//  TASK
//    Remove the restrict qualifiers, and make `copy_words` overlap-safe by
//    choosing the copy direction. Do not change the tests.
//
//  RUN IT
//    ./mec test 03_08
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>

// TODO: the qualifiers promise "no overlap" -- and the second test's caller
// needs overlap. Fix the contract, then honour it.
void copy_words(uint32_t *restrict dst, const uint32_t *restrict src,
                size_t n) {
  for (size_t i = 0; i < n; ++i) {
    dst[i] = src[i];
  }
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
