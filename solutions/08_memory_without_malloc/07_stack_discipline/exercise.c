// Solution -- 08.07 The stack is a budget, so measure it

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// A stand-in for the real stack region: on the board this would be the
// span between `end` and `_estack` in bsp/stm32l476rg.ld. The top of the
// stack is the END of the array, and use grows DOWNWARD from it.

enum { STACK_WORDS = 64 };
#define STACK_PAINT 0xC5C5C5C5u

static uint32_t sim_stack[STACK_WORDS];

void stack_paint(void) {
  for (size_t i = 0; i < STACK_WORDS; ++i) {
    sim_stack[i] = STACK_PAINT;
  }
}

// A pretend workload: consumes `words` words from the top, the way a call
// chain's frames would.
void run_task_using(size_t words) {
  for (size_t i = 0; i < words && i < STACK_WORDS; ++i) {
    sim_stack[STACK_WORDS - 1u - i] = 0xDEADBEEFu ^ (uint32_t)i;
  }
}

size_t stack_high_water_words(void) {
  // Scan from the BOTTOM: everything still wearing the paint was never
  // touched. The high-water mark is what is left -- the deepest the stack
  // has ever reached, not where it is now.
  size_t untouched = 0;
  while (untouched < STACK_WORDS && sim_stack[untouched] == STACK_PAINT) {
    ++untouched;
  }
  return (size_t)STACK_WORDS - untouched;
}

TEST("a freshly painted stack shows zero use") {
  stack_paint();
  CHECK_EQ(stack_high_water_words(), 0u);
}

TEST("usage is measured from the top, in words") {
  stack_paint();
  run_task_using(10);
  CHECK_EQ(stack_high_water_words(), 10u);
}

TEST("it is a HIGH-water mark: a smaller task does not lower it") {
  // No repaint: the 10-word scar from the previous test is still there.
  run_task_using(6);
  CHECK_EQ(stack_high_water_words(), 10u);
}

TEST("repainting resets the measurement") {
  stack_paint();
  CHECK_EQ(stack_high_water_words(), 0u);
  run_task_using(3);
  CHECK_EQ(stack_high_water_words(), 3u);
}
