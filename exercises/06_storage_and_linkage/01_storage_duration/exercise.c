// =============================================================================
//  06.01 -- Storage duration: whose memory is this?
// =============================================================================
//
//  Every object in C has a STORAGE DURATION -- the span of time its memory is
//  guaranteed to exist (Effective C ch. 2, "Storage Duration"):
//
//    automatic   locals; alive until the enclosing block ends. On the stack.
//    static      file-scope objects and `static` locals; alive for the whole
//                program. In .data or .bss (06.04 says which, and what that
//                costs on a flash part).
//    allocated   malloc; alive until free. Chapter 08 is about living
//                without it.
//
//  The classic blunder is handing out a pointer that outlives its object:
//
//      const char *format(int x) {
//        char buffer[16];               // automatic: dies at the brace
//        snprintf(buffer, ...);
//        return buffer;                 // dangling the moment it returns
//      }
//
//  Clang refuses to compile that (-Wreturn-stack-address; CERT DCL30-C), and
//  this course's warning set makes it an error. So far so good. The trouble
//  is the "fix" people then reach for: make the buffer `static`. It compiles.
//  It even works -- until the function is called twice while the first result
//  is still in use, because every call returns THE SAME BYTES. strtok, ctime
//  and asctime are infamous for exactly this design, and it is also why none
//  of them may be called from an ISR (12.06 returns to that).
//
//  That is the starter you have: someone took the compiler's hint, went
//  static, and wired the caller-provided buffer through as decoration. The
//  second test catches them.
//
//  The embedded idiom is the third option: THE CALLER OWNS THE MEMORY. The
//  caller knows the lifetime it needs (a stack scratch here, a static log
//  slot there); the function just fills what it is given. Every serious C
//  API you will meet -- snprintf itself, strftime, the Linux kernel's
//  snprintf-alikes -- has this shape.
//
//  Rule of thumb for what memory to hand it, on a 128 KB-RAM part:
//  small and transient -> stack; big or long-lived -> static, and budgeted.
//
//  TASK
//    Make `format_reading` use the buffer it is given. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 06_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>
#include <stdio.h>

const char *format_reading(char *out, size_t out_size, int centi_c) {
  // TODO: the caller's buffer is ignored; every call returns the same
  // static bytes, so no two results can be alive at once.
  (void)out;
  (void)out_size;
  static char buffer[16];
  int whole = centi_c / 10;
  int tenth = centi_c % 10;
  if (tenth < 0) {
    tenth = -tenth;
  }
  snprintf(buffer, sizeof buffer, "%d.%dC", whole, tenth);
  return buffer;
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
