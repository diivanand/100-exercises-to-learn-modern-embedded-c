// Solution -- 13.06 Debouncing: the integrator

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
  // The integrator: charge towards the raw level, saturating at the rails.
  // Chatter moves it up and down; only a sustained level walks it all the
  // way to a rail, and only touching a rail flips the debounced state.
  if (raw) {
    if (d->integrator < d->threshold) {
      ++d->integrator;
    }
  } else {
    if (d->integrator > 0) {
      --d->integrator;
    }
  }

  if (!d->stable && d->integrator == d->threshold) {
    d->stable = true;
    return EDGE_PRESS;
  }
  if (d->stable && d->integrator == 0) {
    d->stable = false;
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
