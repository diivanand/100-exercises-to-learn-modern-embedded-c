// =============================================================================
//  06.05 -- Static locals: one instance per program, not per user
// =============================================================================
//
//  A `static` inside a function is static storage with function-scope
//  visibility: initialised once, alive for ever, invisible outside. Two
//  facts worth having straight (Effective C ch. 2, "Storage Class"):
//
//   - The initialiser must be a CONSTANT EXPRESSION. C has no C++-style
//     dynamic initialisation of statics -- no hidden guard variable, no
//     thread-safe-init machinery, no code running before main. What you
//     write is baked into .data (or .bss) by the linker; 06.04 showed the
//     mechanism. One more reason C maps so directly onto small hardware.
//
//   - It is ONE object. Not one per caller, not one per channel, not one
//     per thread. One per program.
//
//  The second fact is the trap. A static local is a global with better
//  hygiene, and hiding state in one feels tidy right up until the function
//  gets a second user. The starter below is a moving-average filter that
//  was written for a board with one ADC channel; its window and indices are
//  static locals. The new board smooths two channels, so the API grew a
//  state parameter -- but the statics stayed, and both channels are being
//  stirred into ONE window. Channel A's average is polluted by channel B's
//  samples. (The tests interleave two steady signals and catch the blend.
//  Note which test still passes: with a single caller the bug is
//  invisible, which is exactly how it shipped.)
//
//  The fix is the oldest move in C: PUT THE STATE IN A STRUCT AND HAND IT
//  IN. One struct per channel, owned by the caller, initialised explicitly.
//  Every serious C API is shaped like this, and 03.07's void-pointer
//  context and 10.01's driver instances are the same move again. As a
//  bonus, statics stop being a reentrancy hazard once they stop existing:
//  12.06 shows what a static local does to a function called from an ISR.
//
//  TASK
//    Make smoother_update use the state it is handed. smoother_init should
//    give a fresh, empty filter. Do not change the tests.
//
//  RUN IT
//    ./mec test 06_05
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

enum { WINDOW = 4 };

struct smoother {
  uint16_t window[WINDOW];
  size_t next;
  size_t filled;
};

void smoother_init(struct smoother *s) {
  *s = (struct smoother){0};
}

uint16_t smoother_update(struct smoother *s, uint16_t sample) {
  // TODO: the parameter arrived when the second channel did; the statics
  // are still here from the single-channel days. Every caller shares them.
  (void)s;
  static uint16_t window[WINDOW];
  static size_t next = 0;
  static size_t filled = 0;
  window[next] = sample;
  next = (next + 1) % WINDOW;
  if (filled < WINDOW) {
    ++filled;
  }
  uint32_t sum = 0;
  for (size_t i = 0; i < filled; ++i) {
    sum += window[i];
  }
  return (uint16_t)(sum / filled);
}

TEST("a steady signal converges to its value") {
  struct smoother a;
  smoother_init(&a);
  uint16_t out = 0;
  for (int i = 0; i < 8; ++i) {
    out = smoother_update(&a, 400);
  }
  CHECK_EQ(out, 400u);
}

TEST("two channels do not bleed into each other") {
  struct smoother a;
  struct smoother b;
  smoother_init(&a);
  smoother_init(&b);
  for (int i = 0; i < 4; ++i) {
    smoother_update(&a, 400); // channel A: steady 400
    smoother_update(&b, 100); // channel B: steady 100
  }
  CHECK_EQ(smoother_update(&a, 400), 400u);
  CHECK_EQ(smoother_update(&b, 100), 100u);
}

TEST("a fresh smoother starts from scratch") {
  struct smoother a;
  smoother_init(&a);
  CHECK_EQ(smoother_update(&a, 500), 500u); // average of one sample
}
