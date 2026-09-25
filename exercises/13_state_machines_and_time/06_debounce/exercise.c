// =============================================================================
//  13.06 -- Debouncing: the integrator
// =============================================================================
//
//  A mechanical contact does not close. It CRASHES closed -- metal on
//  metal, bouncing like a dropped plate for anything from a hundred
//  microseconds to tens of milliseconds, and the GPIO faithfully reports
//  every bounce as an edge. Read the pin raw and one press is three; a
//  vending machine dispenses two drinks; a "count the events" test bench
//  disagrees with the operator's finger. Jack Ganssle's field study ("A
//  Guide to Debouncing" -- he measured actual switches) found bounce
//  trains well past 6 ms on ordinary parts; he also wrote the foreword to
//  this course's TDD book, which feels right for the most-tested little
//  algorithm in embedded.
//
//  The cure is never "read it again and hope": it is a FILTER between the
//  raw samples and the logic. Two classics:
//
//   - SHIFT REGISTER: keep the last 8 samples in a byte; pressed when it
//     reads 0xFF, released at 0x00. Simple, fixed window.
//   - INTEGRATOR (this exercise): a saturating counter charges toward the
//     raw level -- up on 1, down on 0, clamped to [0, threshold]. Only a
//     SUSTAINED level walks it to a rail, and only touching a rail flips
//     the debounced state. Chatter jiggles the middle and changes nothing.
//     Tunable (threshold x sample period = the time constant: 4 samples
//     at 5 ms = a 20 ms filter) and immune to a single spike of EMI.
//
//  Keep two ideas separate in the API, because callers need both:
//
//   - LEVEL: is the button pressed right now? (debounce_is_pressed)
//   - EDGE: did it BECOME pressed on this sample? (the return value)
//
//  A superloop's menu logic wants edges; a "hold to power off" wants the
//  level. Conflate them and someone reimplements one from the other,
//  badly, at the call site (13.07 wires this module to a state machine).
//
//  The starter skips the filter entirely and reports an edge on every raw
//  change -- the vending machine build. The tests feed it realistic bounce
//  trains, with the integrator's walk written out sample by sample.
//
//  TASK
//    Implement the integrator in debounce_sample: charge toward `raw`,
//    saturate at the rails, flip `stable` (and return the edge) only at
//    a rail. Do not change the tests.
//
//  RUN IT
//    ./mec test 13_06
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum button_edge {
  EDGE_NONE,
  EDGE_PRESS,
  EDGE_RELEASE,
};

struct debounce {
  uint8_t integrator; // 0 .. threshold, saturating
  uint8_t threshold;
  bool stable; // the debounced LEVEL; edges are derived from its changes
};

void debounce_init(struct debounce *d, uint8_t threshold, bool initially_pressed) {
  d->threshold = threshold;
  d->integrator = initially_pressed ? threshold : 0;
  d->stable = initially_pressed;
}

bool debounce_is_pressed(const struct debounce *d) {
  return d->stable;
}

enum button_edge debounce_sample(struct debounce *d, bool raw) {
  // TODO: no filter -- every raw flicker becomes an event, and the level
  // follows the chatter. The integrator and threshold are sitting in the
  // struct, unused.
  const bool was = d->stable;
  d->stable = raw;
  if (raw && !was) {
    return EDGE_PRESS;
  }
  if (!raw && was) {
    return EDGE_RELEASE;
  }
  return EDGE_NONE;
}

// --- tests --------------------------------------------------------------------

// Run a sample train and tally what comes out.
struct tally {
  unsigned presses;
  unsigned releases;
};

static struct tally run_train(struct debounce *d, const bool *raw, size_t n) {
  struct tally t = {0};
  for (size_t i = 0; i < n; ++i) {
    switch (debounce_sample(d, raw[i])) {
    case EDGE_PRESS:
      ++t.presses;
      break;
    case EDGE_RELEASE:
      ++t.releases;
      break;
    case EDGE_NONE:
      break;
    }
  }
  return t;
}

TEST("a bouncy press produces exactly one press event") {
  struct debounce d;
  debounce_init(&d, 4, false);

  // A real contact closing: chatter, then a solid level.
  //
  //   raw        : 0 1 0 1 1 0 1 1 1 1 1
  //   integrator : 0 1 0 1 2 1 2 3 4 4 4
  //                                ^ press fires here (sample 8), once
  const bool train[] = {0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1};
  const struct tally t = run_train(&d, train, sizeof train / sizeof train[0]);

  CHECK_EQ(t.presses, 1u);
  CHECK_EQ(t.releases, 0u);
  CHECK(debounce_is_pressed(&d));
}

TEST("the level holds steady while the contact is still bouncing") {
  struct debounce d;
  debounce_init(&d, 4, false);

  // Four samples into the chatter (raw went 0,1,0,1) the RAW level is
  // high, but the debounced level must still be "not pressed" -- that is
  // the entire product being purchased here.
  (void)debounce_sample(&d, false);
  (void)debounce_sample(&d, true);
  (void)debounce_sample(&d, false);
  (void)debounce_sample(&d, true);
  CHECK_FALSE(debounce_is_pressed(&d));
}

TEST("a bouncy release produces exactly one release event") {
  struct debounce d;
  debounce_init(&d, 4, true);

  //   raw        : 1 0 1 0 0 1 0 0 0 0
  //   integrator : 4 3 4 3 2 3 2 1 0 0
  //                                ^ release fires here (sample 8), once
  const bool train[] = {1, 0, 1, 0, 0, 1, 0, 0, 0, 0};
  const struct tally t = run_train(&d, train, sizeof train / sizeof train[0]);

  CHECK_EQ(t.presses, 0u);
  CHECK_EQ(t.releases, 1u);
  CHECK_FALSE(debounce_is_pressed(&d));
}

TEST("holding the button generates no further events") {
  struct debounce d;
  debounce_init(&d, 4, false);

  for (int i = 0; i < 10; ++i) {
    (void)debounce_sample(&d, true); // one press somewhere in here
  }
  unsigned extra = 0;
  for (int i = 0; i < 50; ++i) {
    if (debounce_sample(&d, true) != EDGE_NONE) {
      ++extra;
    }
  }
  CHECK_EQ(extra, 0u);
  CHECK(debounce_is_pressed(&d));
}

TEST("a spike shorter than the threshold never registers") {
  struct debounce d;
  debounce_init(&d, 4, false);

  // Three high samples -- EMI, a scope probe, a cat -- then quiet. Never
  // enough to charge the integrator to 4: no event, no level change.
  const bool train[] = {1, 1, 1, 0, 0, 0, 0, 0};
  const struct tally t = run_train(&d, train, sizeof train / sizeof train[0]);

  CHECK_EQ(t.presses, 0u);
  CHECK_EQ(t.releases, 0u);
  CHECK_FALSE(debounce_is_pressed(&d));
}
