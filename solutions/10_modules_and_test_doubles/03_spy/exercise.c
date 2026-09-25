// Solution -- 10.03 The spy: assert on what the code DID

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
    // The scheduler's whole authority: this one dispatch. It touches the
    // scheduled light and NOTHING else -- the spy below can prove it.
    if (slots[i].turn_on) {
      controller->on(slots[i].light_id);
    } else {
      controller->off(slots[i].light_id);
    }
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
