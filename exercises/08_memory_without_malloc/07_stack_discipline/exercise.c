// =============================================================================
//  08.07 -- The stack is a budget, so measure it
// =============================================================================
//
//  One allocator remains, and you do not get to refuse it: every local
//  variable, every call frame, every interrupt lives on the stack. On the
//  desktop the stack is megabytes backed by guard pages -- overrun it and
//  the OS delivers a tidy SIGSEGV. On the STM32L476 it is whatever
//  bsp/stm32l476rg.ld reserved (`_min_stack_size = 0x800` -- two
//  kilobytes) and there is no guard page: overrun it and the stack walks
//  straight into .bss, silently corrupting whichever globals the linker
//  placed highest. The symptom appears far from the cause; these are the
//  worst bugs firmware has.
//
//  So embedded practice treats stack as a BUDGET, enforced three ways:
//
//   - BAN THE UNBOUNDED. No recursion without a proven depth bound (MISRA
//     C:2012 rule 17.2 bans it outright), no variable-length arrays (our
//     build makes -Wvla an error), no alloca, no `uint8_t frame[2048]` as
//     a local -- big buffers go static (the trade: reentrancy, 12.06).
//   - REMEMBER THE INTERRUPTS. The worst case is the deepest call chain
//     PLUS every interrupt frame that can stack on top of it -- on a
//     Cortex-M4 with FPU context, 26 words per preempting handler before
//     the handler's own locals.
//   - MEASURE. Fill the stack region with a known pattern at boot, let
//     the system run, and see how much paint was scrubbed off. FreeRTOS's
//     uxTaskGetStackHighWaterMark is exactly this; 00.03's canary is its
//     one-word cousin. This exercise builds the technique against a
//     simulated stack; on the board, chapter 15's symbols (`end`,
//     `_estack`) bound the real one.
//
//  The measurement's two conventions trip people reliably: the stack TOP
//  is the highest address (use grows DOWNWARD from the end), and the
//  answer wanted is the HIGH-WATER MARK -- the deepest use since the last
//  paint, not the current depth. Once painted, a scrubbed word stays
//  scrubbed; that is what makes the technique cheap enough to leave
//  running in production.
//
//  TASK
//    stack_high_water_words counts the painted words correctly and then
//    returns the wrong thing: the MARGIN (words never touched) instead of
//    the USAGE. Fix it. Do not change the tests.
//
//  RUN IT
//    ./mec test 08_07
//
// =============================================================================

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
  size_t untouched = 0;
  while (untouched < STACK_WORDS && sim_stack[untouched] == STACK_PAINT) {
    ++untouched;
  }
  // TODO: this is the MARGIN -- the words still wearing their paint. The
  // caller asked how deep the stack GOT.
  return untouched;
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
