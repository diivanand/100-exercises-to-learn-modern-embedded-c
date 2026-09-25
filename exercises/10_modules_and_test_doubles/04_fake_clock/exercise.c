// =============================================================================
//  10.04 -- Control time, or it controls you
// =============================================================================
//
//  Half of firmware is timing: debounce windows, repeat rates, timeouts,
//  blink patterns. Test it against the REAL clock and you get suites that
//  sleep for seconds, pass on Tuesday, and fail under load. Grenning's rule
//  (ch. 8, "Controlling the Clock"): time is an input like any other, so
//  INJECT it. The module asks a `millis` function pointer; production hands
//  it the SysTick counter; the test hands it a fake that reads a variable.
//  Advancing time becomes `fake_now = 500;` -- instant, exact, repeatable.
//
//  Never sleep() in a test. A suite that waits 500 ms to test a 500 ms
//  delay has confused simulating time with wasting it.
//
//  The module under test is a keyboard auto-repeat: fire on the press edge,
//  again after 500 ms of hold, then every 100 ms. The starter fails two
//  timing tests, each a small classic:
//
//   1. THE BOUNDARY. It asks `elapsed > delay` where the contract says the
//      repeat lands ON the deadline (`>=`). One millisecond of nothing --
//      invisible on a bench, obvious to a fake clock that can poll at
//      exactly t = 500.
//
//   2. THE DRIFT. On each repeat it schedules the next one from `now` --
//      the moment it happened to be POLLED -- instead of from the deadline
//      it was serving. Polled 50 ms late, the whole beat slides 50 ms.
//      Late-but-steady is what real superloops deliver (13.07), so the
//      test polls late on purpose and listens for the grid: 1600, 1700,
//      1800. Deadlines advance by `+= PERIOD` from the deadline; `now` is
//      only for asking whether it has arrived.
//
//  (The `(int32_t)(now - deadline) >= 0` form is the wraparound-safe
//  comparison; 13.04 gives it a full exercise.)
//
//  TASK
//    Rework keyrepeat_update to hit the boundary and hold the beat. The
//    solution keeps ONE deadline variable and no separate "repeating"
//    phase -- let the state you store be the state you need.
//
//  RUN IT
//    ./mec test 10_04
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- keyrepeat module ---------------------------------------------------------

#define KEYREPEAT_DELAY_MS 500u
#define KEYREPEAT_PERIOD_MS 100u

static uint32_t (*clock_ms)(void);
static bool was_pressed;
static bool repeating;
static uint32_t pressed_at;
static uint32_t last_fire_at;

void keyrepeat_init(uint32_t (*millis_fn)(void)) {
  clock_ms = millis_fn;
  was_pressed = false;
  repeating = false;
}

bool keyrepeat_update(bool pressed) {
  const uint32_t now = clock_ms();

  if (!pressed) {
    was_pressed = false;
    repeating = false;
    return false;
  }

  if (!was_pressed) { // press edge: fire immediately
    was_pressed = true;
    pressed_at = now;
    return true;
  }

  if (!repeating) {
    // TODO: the contract says the first repeat lands ON the deadline.
    if (now - pressed_at > KEYREPEAT_DELAY_MS) {
      repeating = true;
      last_fire_at = now;
      return true;
    }
    return false;
  }

  // TODO: anchoring to `now` lets every late poll push the beat later.
  if (now - last_fire_at > KEYREPEAT_PERIOD_MS) {
    last_fire_at = now;
    return true;
  }
  return false;
}

// --- tests: the clock is a variable ---------------------------------------------

static uint32_t fake_now;

static uint32_t fake_millis(void) {
  return fake_now;
}

static void fresh_handler_at(uint32_t start_ms) {
  keyrepeat_init(fake_millis);
  fake_now = start_ms;
}

TEST("a fresh press fires immediately, once") {
  fresh_handler_at(0);
  CHECK(keyrepeat_update(true));
  CHECK_FALSE(keyrepeat_update(true)); // same instant: no double fire
}

TEST("the first repeat lands exactly on the deadline") {
  fresh_handler_at(0);
  CHECK(keyrepeat_update(true));
  fake_now = 499;
  CHECK_FALSE(keyrepeat_update(true));
  fake_now = 500; // the boundary belongs to the repeat
  CHECK(keyrepeat_update(true));
  fake_now = 600;
  CHECK(keyrepeat_update(true));
}

TEST("late polling does not push the beat (no drift)") {
  fresh_handler_at(1000);
  CHECK(keyrepeat_update(true));
  fake_now = 1500;
  CHECK(keyrepeat_update(true)); // first repeat, next due 1600
  fake_now = 1650;               // polled 50 ms late
  CHECK(keyrepeat_update(true)); // catches the 1600 beat; next due 1700
  fake_now = 1700;
  CHECK(keyrepeat_update(true)); // still on the grid
  fake_now = 1799;
  CHECK_FALSE(keyrepeat_update(true));
  fake_now = 1800;
  CHECK(keyrepeat_update(true));
}

TEST("release resets: the next press is a fresh press") {
  fresh_handler_at(0);
  CHECK(keyrepeat_update(true));
  fake_now = 700;
  CHECK_FALSE(keyrepeat_update(false));
  fake_now = 710;
  CHECK(keyrepeat_update(true)); // fires immediately again
  fake_now = 1200;               // held 490 ms: not yet
  CHECK_FALSE(keyrepeat_update(true));
  fake_now = 1210; // held 500 ms exactly
  CHECK(keyrepeat_update(true));
}
