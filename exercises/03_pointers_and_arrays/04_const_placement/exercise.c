// =============================================================================
//  03.04 -- const placement: promises read right to left
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR -- the tests hand a const
//  table to functions that did not promise to leave it alone.
//
//  Read pointer declarations RIGHT TO LEFT and const stops being confusing:
//
//      const int16_t *p;         // p is a pointer to a const int16_t:
//                                //   *p = 1;  error     p = q;  fine
//      int16_t *const p;         // p is a const pointer to int16_t:
//                                //   *p = 1;  fine      p = q;  error
//      const int16_t *const p;   // both frozen
//
//  The one that matters for API design is the first. A parameter declared
//  `const T *` makes a promise -- "this function looks, it does not touch"
//  -- and the compiler enforces the promise on the function's own body.
//  Callers can then pass anything: writable buffers, and, crucially for this
//  course, tables declared `const` that the linker placed in FLASH. On the
//  STM32L476RG your constant data lives in read-only memory at 0x08000000;
//  RAM is 24 times scarcer than flash on that part, so `const` on a table is
//  not style, it is where the bytes go (06.04 makes this concrete).
//
//  A parameter MISSING its const poisons every const caller: the tests
//  below define the calibration table `static const`, and handing it to
//  `int16_t *` "discards qualifiers" -- the error you are looking at. There
//  are two ways to make that error go away:
//
//    - fix the SIGNATURE, which is the fix;
//    - cast the const away at the call site, which compiles, lies, and is
//      undefined behaviour the moment anything writes (CERT EXP40-C, and
//      -Wcast-qual is an error in this course for exactly that reason).
//
//  While you are in the signatures: notice neither function has any reason
//  to modify the table. That is the common case. In real code bases, `const
//  T *` is the DEFAULT for any pointer parameter that is only read; the
//  non-const version is the one that should have to justify itself.
//
//  TASK
//    Const-correct the two signatures. The bodies are already right.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 03_04
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// Calibration lookup, clamped: an index off the end reads the last entry.
// TODO: this signature claims the right to modify the table.
int16_t cal_lookup(int16_t *table, size_t len, size_t idx) {
  if (idx >= len) {
    idx = len - 1;
  }
  return table[idx];
}

// TODO: same problem.
int16_t cal_min(int16_t *table, size_t len) {
  int16_t min = table[0];
  for (size_t i = 1; i < len; ++i) {
    if (table[i] < min) {
      min = table[i];
    }
  }
  return min;
}

static const int16_t cal[] = {-120, -60, 0, 60, 120};

TEST("lookup reads the const calibration table") {
  CHECK_EQ(cal_lookup(cal, 5, 2), 0);
  CHECK_EQ(cal_lookup(cal, 5, 0), -120);
  CHECK_EQ(cal_lookup(cal, 5, 99), 120); // clamped to the last entry
}

TEST("min scans the const table") {
  CHECK_EQ(cal_min(cal, 5), -120);
}
