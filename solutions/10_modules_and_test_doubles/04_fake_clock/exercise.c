// Solution -- 10.04 Control time, or it controls you

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// --- keyrepeat module ---------------------------------------------------------

#define KEYREPEAT_DELAY_MS 500u
#define KEYREPEAT_PERIOD_MS 100u

static uint32_t (*clock_ms)(void);
static bool was_pressed;
static uint32_t next_fire_at;

void keyrepeat_init(uint32_t (*millis_fn)(void)) {
  clock_ms = millis_fn;
  was_pressed = false;
}

bool keyrepeat_update(bool pressed) {
  const uint32_t now = clock_ms();

  if (!pressed) {
    was_pressed = false;
    return false;
  }

  if (!was_pressed) { // press edge: fire immediately, schedule the first repeat
    was_pressed = true;
    next_fire_at = now + KEYREPEAT_DELAY_MS;
    return true;
  }

  // ">= 0 via signed difference" is the wraparound-safe way to ask "is the
  // deadline due?" -- 13.04 makes a whole exercise of it.
  if ((int32_t)(now - next_fire_at) >= 0) {
    // Advance from the DEADLINE, not from `now`: if we were polled late,
    // the beat stays on the 100 ms grid instead of drifting by our latency.
    next_fire_at += KEYREPEAT_PERIOD_MS;
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
