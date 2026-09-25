// =============================================================================
//  09.05 -- assert is for bugs, error handling is for life
// =============================================================================
//
//  Two kinds of "cannot happen" get confused constantly, and the confusion
//  only shows up in release builds:
//
//   - A BROKEN INVARIANT: the code itself is wrong. `count` exceeds the
//     capacity it can never exceed. That is what `assert()` is for: check
//     it loudly in development, and let the check vanish under -DNDEBUG --
//     behaviour must not depend on it (Effective C ch. 11, "Assertions";
//     CERT MSC11-C).
//
//   - A RUNTIME CONDITION: the world did something inconvenient. The queue
//     is full because producers outpaced consumers this millisecond. That
//     is LIFE, it happens in the field, and it gets real error handling
//     (09.01) in EVERY build mode.
//
//  Route life through assert and your release build stops checking it.
//  That is the starter's first bug: `ASSERT(q.count < QUEUE_CAP)` is the
//  only thing standing between a full queue and the memory after it. In
//  release mode the check evaporates and the push scribbles past the
//  storage -- onto the canary here; onto whatever the linker placed there
//  on your board.
//
//  The second bug is subtler: `ASSERT(++total_pushed > 0)`. The increment
//  lives INSIDE the assertion, so in release builds the statistics simply
//  stop counting (CERT EXP31-C, "Avoid side effects in assertions"). An
//  assert must be REMOVABLE: evaluating it zero times and one time must
//  leave the program in the same state.
//
//  (Two practical notes. First: this exercise fakes assert with a macro
//  honouring a runtime `release_mode` flag, so ONE binary can demonstrate
//  both modes -- real NDEBUG is a compile-time fork. The semantics match
//  where it matters: in release mode the condition is NOT evaluated.
//  Second: on the target, a failed assert has no stderr to print to.
//  Firmware defines its own: breakpoint under a debugger (`__asm("bkpt")`),
//  a logged code plus watchdog reset in the field. Chapter 15's BSP takes
//  a position. What never changes: asserts check BUGS. And what need not
//  wait for runtime at all goes to _Static_assert -- 07.05.)
//
//  TASK
//    Give the full queue a real error return in all modes, move the
//    statistics out of the assertion, and keep ASSERT for an actual
//    invariant. Do not change the tests.
//
//  RUN IT
//    ./mec test 09_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- a stand-in for assert(), so one binary can probe both build modes -------

static bool release_mode = false;
static unsigned assert_violations = 0;

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!release_mode) {                                                       \
      if (!(cond)) {                                                           \
        ++assert_violations;                                                   \
      }                                                                        \
    }                                                                          \
  } while (0)

// --- a small queue (stack-shaped, for brevity) ---------------------------------

enum queue_status {
  QUEUE_OK = 0,
  QUEUE_FULL,
  QUEUE_EMPTY,
};

#define QUEUE_CAP 8u

static struct {
  int32_t items[QUEUE_CAP];
  int32_t canary; // sits right after the storage; must survive (00.03)
  uint32_t count;
} q = {.canary = 0x5AFE};

static uint32_t total_pushed; // statistics for the telemetry page

static void queue_reset(void) {
  q.count = 0;
  q.canary = 0x5AFE;
  total_pushed = 0;
  assert_violations = 0;
}

enum queue_status queue_push(int32_t v) {
  // TODO: a full queue is not a bug. And find the increment that release
  // builds will delete.
  ASSERT(q.count < QUEUE_CAP);
  ASSERT(++total_pushed > 0);
  q.items[q.count++] = v;
  return QUEUE_OK;
}

enum queue_status queue_pop(int32_t *out) {
  if (q.count == 0) {
    return QUEUE_EMPTY;
  }
  *out = q.items[--q.count];
  return QUEUE_OK;
}

TEST("the queue enforces its capacity with an error, not an assert") {
  queue_reset();
  release_mode = false;
  for (int32_t i = 0; i < (int32_t)QUEUE_CAP; ++i) {
    CHECK_EQ(queue_push(i), QUEUE_OK);
  }
  CHECK_EQ(queue_push(99), QUEUE_FULL);
  CHECK_EQ(q.canary, 0x5AFE);
  CHECK_EQ(assert_violations, 0u); // normal operation breaks no invariant
}

TEST("error handling does not evaporate in release builds") {
  queue_reset();
  release_mode = true; // "compiled with -DNDEBUG"
  for (int32_t i = 0; i < (int32_t)QUEUE_CAP; ++i) {
    CHECK_EQ(queue_push(i), QUEUE_OK);
  }
  CHECK_EQ(queue_push(99), QUEUE_FULL); // still an error, not a scribble
  CHECK_EQ(q.canary, 0x5AFE);
  release_mode = false;
}

TEST("statistics survive the build mode, because they are not in an assert") {
  queue_reset();
  release_mode = false;
  (void)queue_push(1);
  (void)queue_push(2);
  (void)queue_push(3);
  CHECK_EQ(total_pushed, 3u);

  queue_reset();
  release_mode = true;
  (void)queue_push(1);
  (void)queue_push(2);
  (void)queue_push(3);
  CHECK_EQ(total_pushed, 3u); // an assert-resident ++ would read 0 here
  release_mode = false;
}

TEST("pop drains what push stored") {
  queue_reset();
  (void)queue_push(10);
  (void)queue_push(20);
  int32_t v = 0;
  CHECK_EQ(queue_pop(&v), QUEUE_OK);
  CHECK_EQ(v, 20);
  CHECK_EQ(queue_pop(&v), QUEUE_OK);
  CHECK_EQ(v, 10);
  CHECK_EQ(queue_pop(&v), QUEUE_EMPTY);
}
