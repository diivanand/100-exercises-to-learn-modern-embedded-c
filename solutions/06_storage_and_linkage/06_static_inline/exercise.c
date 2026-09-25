// Solution -- 06.06 static inline: the function that costs like a macro

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
// filter.h -- the helper, as it should be
// ---------------------------------------------------------------------------

// static inline: argument evaluated exactly once, types checked, steps
// through cleanly in a debugger -- and at -O2 the call disappears just as
// completely as the macro did. This is the idiom vendor headers use for
// register helpers, and the reason is everything the macro below got wrong.
static inline int32_t clamp12(int32_t x) {
  if (x > 4095) {
    return 4095;
  }
  if (x < 0) {
    return 0;
  }
  return x;
}

uint16_t next_clamped_sample(void) {
  return (uint16_t)clamp12(read_next_raw());
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
