// Solution -- 09.05 assert is for bugs, error handling is for life

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- a stand-in for assert(), so one binary can probe both build modes -------
//
// Real assert() prints and aborts, and vanishes entirely under -DNDEBUG.
// This one records instead of aborting and takes its "NDEBUG" from a
// runtime flag -- same semantics where they matter: in release mode the
// CONDITION IS NOT EVALUATED AT ALL.

static bool release_mode = false;
static unsigned assert_violations = 0;

#define ASSERT(cond)                                                                     \
  do {                                                                                   \
    if (!release_mode) {                                                                 \
      if (!(cond)) {                                                                     \
        ++assert_violations;                                                             \
      }                                                                                  \
    }                                                                                    \
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
  // A full queue is LIFE, not a bug: producers outpace consumers in normal
  // operation. Life gets an error return, in every build mode.
  if (q.count >= QUEUE_CAP) {
    return QUEUE_FULL;
  }
  ++total_pushed; // side effects live OUTSIDE assertions (CERT EXP31-C)
  q.items[q.count++] = v;
  // THIS is what assert is for: an invariant that cannot fail unless the
  // code above it is wrong. It documents, it checks in development, and it
  // may vanish in release without changing behaviour.
  ASSERT(q.count <= QUEUE_CAP);
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
