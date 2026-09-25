// =============================================================================
//  10.03 -- The spy: assert on what the code DID
// =============================================================================
//
//  A fake answers questions (10.02's ADC). A SPY does the opposite job: it
//  sits where an OUTPUT would go and writes down everything the code under
//  test commands -- every call, in order, with arguments. The test then
//  asserts on the recording. Grenning builds his LightControllerSpy for
//  exactly this (ch. 8): a home-automation scheduler whose only observable
//  effect is which lights it switches.
//
//  The discipline that makes a spy worth having: RECORD EVERYTHING. A spy
//  that only remembers the last call answers "did the right light turn
//  on?", but stays silent on the more important question -- "did anything
//  ELSE happen?" Firmware that does the right thing plus one extra thing is
//  firmware with a bug; lights flicking on in an empty house get noticed.
//  So the spy here keeps the full event tape, and the key test asserts the
//  COUNT as well as the content.
//
//  That test is not decoration. The starter's wake loop dispatches the
//  scheduled light correctly -- and then also "tidies an indicator",
//  calling off() with the slot INDEX where a light ID belongs. Index and
//  id are both small integers; the compiler is perfectly happy; light 0 in
//  the hallway is not. Only the everything-recording spy notices the stray
//  command. (This id-for-index confusion is a top-five firmware bug; the
//  cure is the spy's paranoia plus, in bigger code bases, distinct types.)
//
//  Inputs get fakes, outputs get spies; both stay dumb. Logic belongs in
//  the module under test, never in the double -- a clever spy is a second
//  implementation to debug.
//
//  TASK
//    Fix light_scheduler_wake so precisely the scheduled commands cross the
//    boundary -- nothing more. Do not change the tests.
//
//  RUN IT
//    ./mec test 10_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// --- light_controller interface ---------------------------------------------------

struct light_ops {
  void (*on)(uint8_t id);
  void (*off)(uint8_t id);
};

// --- light_scheduler module --------------------------------------------------------

enum { SCHEDULER_MAX_SLOTS = 8 };

struct slot {
  bool used;
  uint8_t light_id;
  uint32_t minute;
  bool turn_on;
};

static const struct light_ops *controller;
static struct slot slots[SCHEDULER_MAX_SLOTS];

void light_scheduler_init(const struct light_ops *ops) {
  controller = ops;
  for (size_t i = 0; i < SCHEDULER_MAX_SLOTS; ++i) {
    slots[i].used = false;
  }
}

bool light_scheduler_add(uint8_t light_id, uint32_t minute, bool turn_on) {
  for (size_t i = 0; i < SCHEDULER_MAX_SLOTS; ++i) {
    if (!slots[i].used) {
      slots[i] = (struct slot){
          .used = true, .light_id = light_id, .minute = minute, .turn_on = turn_on};
      return true;
    }
  }
  return false; // full -- the caller gets to know (09.01)
}

void light_scheduler_wake(uint32_t minute) {
  for (size_t i = 0; i < SCHEDULER_MAX_SLOTS; ++i) {
    if (!slots[i].used || slots[i].minute != minute) {
      continue;
    }
    if (slots[i].turn_on) {
      controller->on(slots[i].light_id);
    } else {
      controller->off(slots[i].light_id);
    }
    // TODO: "reset the slot's activity indicator". That is not what this
    // line does. `i` is a slot INDEX; off() wants a light ID. Somewhere a
    // real light with that number just went dark.
    controller->off((uint8_t)i);
  }
}

// --- tests: a controller made of tape ----------------------------------------------

struct spy_event {
  uint8_t id;
  bool on;
};

enum { SPY_MAX_EVENTS = 16 };
static struct spy_event spy_events[SPY_MAX_EVENTS];
static size_t spy_event_count;

static void spy_record(uint8_t id, bool on) {
  if (spy_event_count < SPY_MAX_EVENTS) {
    spy_events[spy_event_count] = (struct spy_event){.id = id, .on = on};
  }
  ++spy_event_count; // count even past capacity: extra calls must not hide
}

static void spy_on(uint8_t id) {
  spy_record(id, true);
}

static void spy_off(uint8_t id) {
  spy_record(id, false);
}

static const struct light_ops spy_ops = {spy_on, spy_off};

static void fresh_scheduler(void) {
  spy_event_count = 0;
  light_scheduler_init(&spy_ops);
}

TEST("an empty schedule commands nothing") {
  fresh_scheduler();
  light_scheduler_wake(100);
  CHECK_EQ(spy_event_count, 0u);
}

TEST("the scheduled light fires -- and ONLY that light") {
  fresh_scheduler();
  CHECK(light_scheduler_add(3, 100, true));
  light_scheduler_wake(99);
  CHECK_EQ(spy_event_count, 0u);
  light_scheduler_wake(100);
  CHECK_EQ(spy_event_count, 1u); // exactly one command crossed the boundary
  CHECK_EQ(spy_events[0].id, 3u);
  CHECK(spy_events[0].on);
}

TEST("slots fire in the order they were added") {
  fresh_scheduler();
  CHECK(light_scheduler_add(1, 60, true));
  CHECK(light_scheduler_add(2, 60, false));
  light_scheduler_wake(60);
  CHECK_EQ(spy_event_count, 2u);
  CHECK_EQ(spy_events[0].id, 1u);
  CHECK(spy_events[0].on);
  CHECK_EQ(spy_events[1].id, 2u);
  CHECK_FALSE(spy_events[1].on);
}

TEST("a full scheduler says no") {
  fresh_scheduler();
  for (uint8_t i = 0; i < SCHEDULER_MAX_SLOTS; ++i) {
    CHECK(light_scheduler_add(i, 10, true));
  }
  CHECK_FALSE(light_scheduler_add(99, 10, true));
}
