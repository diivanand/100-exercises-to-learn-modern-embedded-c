// =============================================================================
//  07.01 -- Macro hygiene: parentheses, and knowing when to stop
// =============================================================================
//
//  A function-like macro is not a function. It is TEXT SUBSTITUTION, done
//  before the compiler proper ever sees the code, and the two bugs below are
//  the two ways that difference bites (Effective C ch. 9, "Macro
//  Replacement").
//
//  1. MISSING PARENTHESES. After substitution, your expansion is spliced
//     into whatever expression surrounds the call, and the operators there
//     fight the operators here on precedence alone:
//
//         #define CALIBRATE(raw) raw * 4 + 1
//         CALIBRATE(a + b)      // a + b*4 + 1   -- the argument tore apart
//         10 - CALIBRATE(2)     // 10 - 2*4 + 1  -- the expansion tore apart
//
//     The rule (CERT PRE01-C, PRE02-C): parenthesise EVERY use of a
//     parameter, and the WHOLE definition. Both. The inner pair protects
//     against what the caller puts inside the argument; the outer pair
//     against what the caller wraps around the call.
//
//  2. MULTIPLE EVALUATION. However well parenthesised, this
//
//         #define min_u32(a, b) ((a) < (b) ? (a) : (b))
//
//     evaluates one argument twice -- once in the comparison, once in the
//     result. Pass `samples[i++]` and i advances twice; pass `read_fifo()`
//     and you silently pop two entries off a hardware FIFO. That last one is
//     the embedded version of this bug, and people lose days to it: reading
//     a data register CONSUMES data (11.04), and a macro that reads it twice
//     corrupts the stream only when its branch condition is true. CERT
//     PRE31-C: never pass expressions with side effects to an unsafe macro.
//
//     The lowercase name is part of the trap -- it looks like a function and
//     callers will treat it like one. You cannot fix min-as-a-macro with
//     parentheses. Write a `static inline` function instead: same inlining
//     opportunity, but with checked types, single evaluation, and a name the
//     debugger can find. Macros are for what functions cannot do -- token
//     pasting, stringifying, compile-time table generation (07.03, 07.06) --
//     not for arithmetic.
//
//  TASK
//    Fix CALIBRATE's parenthesisation, and replace the min_u32 macro with a
//    static inline function. Do not change the tests.
//
//  RUN IT
//    ./mec test 07_01
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// Calibration for a sensor with gain 4 and offset 1.
// TODO: this expansion loses every precedence fight it is invited to.
#define CALIBRATE(raw) raw * 4 + 1

// TODO: parentheses are not this macro's real problem. Replace it.
#define min_u32(a, b) ((a) < (b) ? (a) : (b))

TEST("calibration survives an expression argument") {
  const int a = 2;
  const int b = 3;
  // (2 + 3) * 4 + 1 = 21. The unparenthesised macro computed 2 + 3*4 + 1.
  CHECK_EQ(CALIBRATE(a + b), 21);
}

TEST("calibration survives an expression around the call") {
  // 10 - CALIBRATE(2) = 10 - 9 = 1. Without the outer parentheses the
  // expansion was 10 - 2*4 + 1 = 3.
  CHECK_EQ(10 - CALIBRATE(2), 1);
}

TEST("min evaluates each argument exactly once") {
  uint32_t samples[] = {3, 7};
  size_t i = 0;
  const uint32_t m = min_u32(samples[i++], 5u);
  CHECK_EQ(m, 3u); // min(3, 5)
  CHECK_EQ(i, (size_t)1); // i++ must have happened once, not twice
}

TEST("min still behaves like min") {
  CHECK_EQ(min_u32(5u, 9u), 5u);
  CHECK_EQ(min_u32(9u, 5u), 5u);
  CHECK_EQ(min_u32(4u, 4u), 4u);
}
