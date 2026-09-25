// =============================================================================
//  07.02 -- Multi-statement macros and do { } while (0)
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR ("expected expression" /
//  an `else` with no `if` to belong to). The error is the lesson.
//
//  A macro that expands to TWO statements is a trap armed by the caller's
//  formatting. RECORD_FAULT below expands to
//
//      fault_count++; last_fault = (code)
//
//  and at the call site inside an unbraced if/else:
//
//      if (temp_c > 85)
//        RECORD_FAULT(FAULT_OVER_TEMP);   // expands to TWO statements
//      else
//        ok_count++;
//
//  only the FIRST statement is governed by the `if`. The second sits after
//  it, unconditionally -- and now the `else` has no `if` to attach to, which
//  is the compile error you are looking at. You were lucky: had there been
//  no `else`, this would have compiled and quietly recorded `last_fault` on
//  EVERY reading, healthy or not.
//
//  Braces alone do not fix it. `{ fault_count++; last_fault = (code); }`
//  makes the if/else parse -- until you notice the caller writes a semicolon
//  after the macro call, `RECORD_FAULT(x);`, and `{ ... };` is a compound
//  statement PLUS an empty statement. Two statements again; the else breaks
//  again.
//
//  The idiom that survives every context is
//
//      #define RECORD_FAULT(code)  do { ...; ...; } while (0)
//
//  A do-while is grammatically ONE statement, and it is the one loop form
//  that DEMANDS a trailing semicolon -- so `RECORD_FAULT(x);` parses exactly
//  like a function call, in an unbraced if, an else, a loop body, anywhere.
//  The generated code is identical to the two bare statements; the loop
//  "runs" once and every compiler folds it away. This is CERT PRE10-C, and
//  you will find the idiom in every serious C code base -- including the
//  Linux kernel and this course's own harness (mect.h, the REQUIRE macro).
//
//  (GCC and Clang also offer statement expressions, `({ ... })`, which solve
//  this and return a value. They are an extension, not C17; recognise them
//  when you read vendor code, and know they will not port to every embedded
//  toolchain.)
//
//  TASK
//    Repackage RECORD_FAULT so the file compiles and the counters are right.
//    Do not change the tests, and do not change the call sites -- unbraced
//    if/else is exactly what the macro must survive.
//
//  RUN IT
//    ./mec test 07_02
//
// =============================================================================

#include <mect/mect.h>

enum fault_code { FAULT_NONE, FAULT_OVER_TEMP, FAULT_UNDER_VOLT };

static unsigned fault_count = 0;
static enum fault_code last_fault = FAULT_NONE;
static unsigned ok_count = 0;

// TODO: two statements pretending to be one.
#define RECORD_FAULT(code)                                                     \
  fault_count++;                                                               \
  last_fault = (code)

static void reset_counters(void) {
  fault_count = 0;
  last_fault = FAULT_NONE;
  ok_count = 0;
}

static void check_temperature(int temp_c) {
  if (temp_c > 85)
    RECORD_FAULT(FAULT_OVER_TEMP);
  else
    ok_count++;
}

static void check_supply(int millivolts) {
  if (millivolts < 3000)
    RECORD_FAULT(FAULT_UNDER_VOLT);
  else
    ok_count++;
}

TEST("a fault is recorded with its code") {
  reset_counters();
  check_temperature(90);
  CHECK_EQ(fault_count, 1u);
  CHECK_EQ((int)last_fault, (int)FAULT_OVER_TEMP);
  CHECK_EQ(ok_count, 0u);
}

TEST("a healthy reading takes the else branch") {
  reset_counters();
  check_temperature(25);
  CHECK_EQ(fault_count, 0u);
  CHECK_EQ(ok_count, 1u);
}

TEST("a mixed sequence keeps both counters honest") {
  reset_counters();
  check_temperature(25);  // ok
  check_supply(2800);     // fault
  check_temperature(100); // fault
  check_supply(3300);     // ok
  CHECK_EQ(fault_count, 2u);
  CHECK_EQ(ok_count, 2u);
  CHECK_EQ((int)last_fault, (int)FAULT_OVER_TEMP);
}
