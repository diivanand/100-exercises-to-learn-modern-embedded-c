// =============================================================================
//  06.06 -- static inline: the function that costs like a macro
// =============================================================================
//
//  For thirty years the excuse for function-like macros was speed: a macro
//  pastes its body at the call site, a function pays a call. On a 72 MHz
//  part servicing a 1 MHz interrupt, that argument mattered. It has been
//  dead since compilers learned to inline -- what remains is the price:
//
//      #define CLAMP12(x) ((x) > 4095 ? 4095 : ((x) < 0 ? 0 : (x)))
//
//  Count the x's. THREE. A macro does not take an argument, it repeats the
//  TEXT of one, so `CLAMP12(read_next_raw())` reads the ADC up to three
//  times and hands you a value assembled from different samples. No type
//  checking either -- pass a double, get a double -- and nothing sensible
//  to step through in a debugger. MISRA C (Dir 4.9) says to prefer
//  functions; chapter 07 opens with the full macro-hygiene toolkit for the
//  macros you genuinely cannot avoid.
//
//  The modern replacement:
//
//      static inline int32_t clamp12(int32_t x) { ... }
//
//  Argument evaluated once. Types checked. Debuggable at -O0. Gone -- fully
//  pasted, no call -- at -O2. This is what vendor headers (CMSIS, every
//  HAL) use for register helpers, and it is the embedded default for any
//  small hot helper.
//
//  Why `static` inline and not plain `inline`? C99's inline model is a
//  trap for the unwary: a plain `inline` function in C provides an INLINE
//  DEFINITION but no external one, and if the compiler chooses NOT to
//  inline a call (at -O0, say, or a call through a function pointer), the
//  linker goes looking for an external definition that no file provides --
//  "undefined reference", but only in some build configurations. (C++
//  solved this with a comdat scheme; C did not.) You are supposed to
//  provide `extern inline` in exactly one .c file. Nobody remembers the
//  incantation, and nobody needs to: `static inline` gives every including
//  file its own private copy, identical machine code at -O2, no linker
//  drama. That is why it is the only spelling you will see in headers that
//  ship.
//
//  The starter is the macro, live, wired to a scripted ADC that counts its
//  reads. Watch the first test: the samples come out WRONG -- every third
//  one -- because the macro ate two extra readings deciding what to do
//  with the first.
//
//  TASK
//    Replace the CLAMP12 macro with a static inline clamp12 function and
//    use it. Do not change the tests.
//
//  RUN IT
//    ./mec test 06_06
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// adc.c -- test fixture: a scripted ADC that counts how often it is read
// ---------------------------------------------------------------------------
static const int32_t *script;
static size_t script_len;
static size_t script_pos;
static unsigned reads;

static void feed_samples(const int32_t *samples, size_t n) {
  script = samples;
  script_len = n;
  script_pos = 0;
  reads = 0;
}

static unsigned reads_taken(void) {
  return reads;
}

static int32_t read_next_raw(void) {
  ++reads;
  if (script_pos >= script_len) {
    return 0; // scripted ADC ran dry: a read past the plan reads as 0
  }
  return script[script_pos++];
}

// ---------------------------------------------------------------------------
// filter.h -- the helper, as it should not be
// ---------------------------------------------------------------------------

// TODO: three evaluations of x, no types, nothing to breakpoint.
#define CLAMP12(x) ((x) > 4095 ? 4095 : ((x) < 0 ? 0 : (x)))

uint16_t next_clamped_sample(void) {
  return (uint16_t)CLAMP12(read_next_raw());
}

TEST("in-range samples pass through, in order") {
  const int32_t s[] = {10, 20, 30, 40, 50, 60};
  feed_samples(s, 6);
  CHECK_EQ(next_clamped_sample(), 10u);
  CHECK_EQ(next_clamped_sample(), 20u);
}

TEST("exactly one ADC read per sample") {
  const int32_t s[] = {10, 20, 30, 40};
  feed_samples(s, 4);
  next_clamped_sample();
  next_clamped_sample();
  CHECK_EQ(reads_taken(), 2u);
}

TEST("out-of-range samples clamp to 12 bits") {
  const int32_t s[] = {9999, -5, 100};
  feed_samples(s, 3);
  CHECK_EQ(next_clamped_sample(), 4095u);
  CHECK_EQ(next_clamped_sample(), 0u);
  CHECK_EQ(next_clamped_sample(), 100u);
}
