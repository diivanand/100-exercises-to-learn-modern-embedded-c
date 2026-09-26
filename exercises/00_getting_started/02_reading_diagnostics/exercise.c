// =============================================================================
//  00.02 -- Take the compiler seriously
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR. That is deliberate; the
//  errors are the exercise.
//
//  This course builds with an aggressive warning set, and warnings are
//  errors (see cmake/CompilerWarnings.cmake -- every flag has a comment).
//  That is not pedantry. The compiler is the cheapest static analyser you
//  will ever run, it runs on every build, and in C -- a language that will
//  happily let you index past the end of an array -- it is the first line of
//  defence. Effective C (ch. 11) opens its tooling advice with exactly this;
//  MISRA C, the automotive C standard, effectively requires it.
//
//  Three problems are waiting below. (The compiler will show you more than
//  three MESSAGES -- an undeclared function drags consequences behind it.
//  Standard advice for C diagnostics: fix the FIRST error, rebuild, and see
//  what is really left.) In the order you will meet them:
//
//  1. AN UNDECLARED FUNCTION. C reads a file strictly top to bottom: at the
//     call site of `read_be16` the compiler has never heard of it, and since
//     C99 that is an error, not a guess. (C89 would silently *invent* a
//     declaration -- `int read_be16()` -- and miscompile the call. That
//     "implicit int" era is where much of C's bad reputation comes from.)
//     Fix it with a PROTOTYPE above the first use, or by reordering.
//
//  2. A FLOAT WHERE YOU MEANT INTEGERS. `-Wconversion` flags the implicit
//     double-to-int conversion in `battery_percent`. The warning is the
//     visible symptom; the deeper problem is the `100.0`. This course runs
//     on parts where `double` is software-emulated (the Cortex-M4F has a
//     single-precision FPU only) -- a float slipping into integer code costs
//     kilobytes of libgcc and microseconds per call. Do the scaling in
//     integer arithmetic.
//
//  3. AN UNUSED PARAMETER -- the most interesting of the three, because here
//     the warning is pointing at a REAL BUG: `pack_status` ignores its
//     `link_ok` argument, and the tests below can tell. An unused-parameter
//     warning is sometimes noise (the parameter exists to satisfy a callback
//     signature -- 03.06 shows the `(void)param;` idiom for saying so), but
//     read it before you silence it. Sometimes it is the bug.
//
//  TASK
//    Make it compile, then make it pass. Do not change the tests.
//
//  RUN IT
//    ./mec test 00_02
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

static uint16_t read_be16(const uint8_t *b);

// A big-endian 16-bit message id sits in the first two bytes of a frame.
uint16_t message_id(const uint8_t *frame) {
  return read_be16(frame);
}

static uint16_t read_be16(const uint8_t *b) {
  return (uint16_t)((uint16_t)(b[0] << 8) | b[1]);
}

// A fuel gauge reports 0..255; the UI wants 0..100.
uint8_t battery_percent(uint8_t raw) {
  return (uint8_t)((uint32_t)raw * 100U / 255U);
}

// Status byte: bit 7 = link up, bits 6..0 = battery percent (0..100).
uint8_t pack_status(uint8_t percent, uint8_t link_ok) {
  uint8_t bit7_val = link_ok == 0 ? 0 : 1U << 7U;
  uint8_t mask_keep_bits_0_thru_6 = 0x7F;
  return bit7_val | (percent & mask_keep_bits_0_thru_6);
}

TEST("message id is read big-endian") {
  const uint8_t frame[] = {0x12, 0x34, 0xFF, 0xFF};
  CHECK_EQ(message_id(frame), 0x1234u);
}

TEST("battery percent scales 0..255 to 0..100") {
  CHECK_EQ(battery_percent(255), 100u);
  CHECK_EQ(battery_percent(128), 50u);
  CHECK_EQ(battery_percent(0), 0u);
}

TEST("status byte carries the link bit AND the percent") {
  CHECK_EQ(pack_status(100, 1), 0xE4u); // 0x80 | 100
  CHECK_EQ(pack_status(50, 0), 0x32u);
  CHECK_EQ(pack_status(0, 1), 0x80u);
}
