// Solution -- 13.07 The superloop: everything, on time, forever

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- debounce (13.06's integrator, verbatim) ----------------------------------

enum button_edge { EDGE_NONE, EDGE_PRESS, EDGE_RELEASE };

struct debounce {
  uint8_t integrator;
  uint8_t threshold;
  bool stable;
};

static void debounce_init(struct debounce *d, uint8_t threshold) {
  d->threshold = threshold;
  d->integrator = 0;
  d->stable = false;
}

static bool debounce_is_pressed(const struct debounce *d) {
  return d->stable;
}

static enum button_edge debounce_sample(struct debounce *d, bool raw) {
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

// --- the application ----------------------------------------------------------

enum led_mode { MODE_OFF, MODE_SLOW, MODE_FAST };

#define SLOW_PERIOD 40u
#define FAST_PERIOD 10u
#define APP_STEP_BUDGET 4u // work units; the tests hold every step to this

struct app {
  struct debounce btn;
  enum led_mode mode;
  bool led_on;
  uint32_t blink_due;
  unsigned toggles;       // blink-service toggles, for the tests
  unsigned max_step_work; // worst step seen, for the budget check
};

void app_init(struct app *a) {
  *a = (struct app){0};
  debounce_init(&a->btn, 4);
}

static uint32_t blink_period(enum led_mode mode) {
  return mode == MODE_FAST ? FAST_PERIOD : SLOW_PERIOD;
}

// One iteration of the superloop: poll, decide, service deadlines, return.
// Every task runs to completion and nothing waits -- if it is not due yet,
// the answer is "not yet", never "hold on". Returns the work units spent,
// standing in for the cycles a profiler would count.
unsigned superloop_step(struct app *a, bool raw_button, uint32_t now) {
  unsigned work = 0;

  // Task 1: sample the button through the debouncer. One sample per step;
  // the FILTER absorbs the chatter, the LOOP provides the cadence.
  ++work;
  const enum button_edge edge = debounce_sample(&a->btn, raw_button);

  // Task 2: mode logic, on the press edge only.
  if (edge == EDGE_PRESS) {
    ++work;
    switch (a->mode) {
    case MODE_OFF:
      a->mode = MODE_SLOW;
      break;
    case MODE_SLOW:
      a->mode = MODE_FAST;
      break;
    case MODE_FAST:
      a->mode = MODE_OFF;
      break;
    }
    if (a->mode == MODE_OFF) {
      a->led_on = false;
    } else {
      a->led_on = true; // entering a blink mode: visible feedback now,
      a->blink_due = now + blink_period(a->mode); // first toggle one period on
    }
  }

  // Task 3: service the blink deadline (13.04's modular comparison; the
  // schedule advances from `due`, 13.05's no-drift rule).
  if (a->mode != MODE_OFF && now - a->blink_due < 0x80000000u) {
    ++work;
    a->led_on = !a->led_on;
    ++a->toggles;
    a->blink_due += blink_period(a->mode);
  }

  if (work > a->max_step_work) {
    a->max_step_work = work;
  }
  return work;
}

// --- tests --------------------------------------------------------------------

// Drive N steps, 5 ticks apart, with a constant raw button level.
static void feed(struct app *a, bool raw, unsigned steps, uint32_t *now) {
  for (unsigned i = 0; i < steps; ++i) {
    *now += 5;
    (void)superloop_step(a, raw, *now);
  }
}

// A full press-and-release: 4 high samples (the press fires on the 4th),
// then 4 low samples on the way to release.
static void press(struct app *a, uint32_t *now) {
  feed(a, true, 4, now);
  feed(a, false, 4, now);
}

TEST("a bouncy press turns the LED on; the blink keeps schedule") {
  struct app a;
  app_init(&a);
  uint32_t now = 0;

  // Timeline, steps 5 ticks apart. Button raw=1 from step 2 to step 8:
  //
  //   step :  0   1   2   3   4   5   6   7   8   9  10  11  12  13
  //   now  :  5  10  15  20  25  30  35  40  45  50  55  60  65  70
  //   raw  :  0   0   1   1   1   1   1   1   1   0   0   0   0   0
  //   integ:  0   0   1   2   3   4   4   4   4   3   2   1   0   0
  //                            ^ press fires at now=30: SLOW, LED on,
  //                              blink_due = 30 + 40 = 70
  feed(&a, false, 2, &now); // steps 0-1
  feed(&a, true, 3, &now);  // steps 2-4: still charging
  CHECK_EQ((int)a.mode, (int)MODE_OFF);
  CHECK_FALSE(a.led_on);

  feed(&a, true, 4, &now); // steps 5-8: press fires on step 5 (now=30)
  CHECK_EQ((int)a.mode, (int)MODE_SLOW);
  CHECK(a.led_on);
  CHECK_EQ(a.toggles, 0u);

  feed(&a, false, 4, &now); // steps 9-12 (now=65): released, not yet due
  CHECK_FALSE(debounce_is_pressed(&a.btn));
  CHECK(a.led_on);
  CHECK_EQ(a.toggles, 0u);

  feed(&a, false, 1, &now); // step 13, now=70: the first toggle
  CHECK_EQ(a.toggles, 1u);
  CHECK_FALSE(a.led_on);

  feed(&a, false, 8, &now); // now=110: due was 110 -> second toggle
  CHECK_EQ(a.toggles, 2u);
  CHECK(a.led_on);

  CHECK(a.max_step_work <= APP_STEP_BUDGET);
}

TEST("presses cycle OFF -> SLOW -> FAST -> OFF, at their own rates") {
  struct app a;
  app_init(&a);
  uint32_t now = 0;

  press(&a, &now);
  CHECK_EQ((int)a.mode, (int)MODE_SLOW);
  CHECK(a.led_on);

  // SLOW: period 40. Over a 40-tick window, exactly one toggle.
  unsigned t = a.toggles;
  feed(&a, false, 8, &now);
  CHECK_EQ(a.toggles, t + 1u);

  press(&a, &now);
  CHECK_EQ((int)a.mode, (int)MODE_FAST);

  // FAST: period 10. Over a 40-tick window, exactly four.
  t = a.toggles;
  feed(&a, false, 8, &now);
  CHECK_EQ(a.toggles, t + 4u);

  press(&a, &now);
  CHECK_EQ((int)a.mode, (int)MODE_OFF);
  CHECK_FALSE(a.led_on);

  // OFF: the blink task is idle, and stays idle.
  t = a.toggles;
  feed(&a, false, 8, &now);
  CHECK_EQ(a.toggles, t);
  CHECK_FALSE(a.led_on);
}

TEST("a one-step spike is not a press, and no step blows the budget") {
  struct app a;
  app_init(&a);
  uint32_t now = 0;

  // One high sample -- EMI, not a finger. The debouncer must absorb it
  // ACROSS steps; a step that "waits to be sure" instead has stopped
  // being a superloop.
  feed(&a, false, 1, &now);
  feed(&a, true, 1, &now);
  feed(&a, false, 6, &now);

  CHECK_EQ((int)a.mode, (int)MODE_OFF);
  CHECK_FALSE(a.led_on);
  CHECK(a.max_step_work <= APP_STEP_BUDGET);
}
