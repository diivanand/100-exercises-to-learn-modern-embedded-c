// Solution -- 03.06 Function pointers: the dispatch table

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
  EVENT_TICK,
  EVENT_BUTTON,
  EVENT_UART,
  EVENT_COUNT, // trailing counter: grows automatically with the enum
};

typedef void (*event_handler)(uint8_t payload);

static event_handler handlers[EVENT_COUNT];

bool event_register(uint8_t event, event_handler h) {
  // Both checks are the API earning its keep: an out-of-range event would
  // write PAST the table -- over whatever global the linker placed next --
  // and a NULL handler stored now is a crash deferred to dispatch time,
  // which is a far worse place to discover it.
  if (event >= EVENT_COUNT || h == NULL) {
    return false;
  }
  handlers[event] = h;
  return true;
}

bool event_dispatch(uint8_t event, uint8_t payload) {
  if (event >= EVENT_COUNT || handlers[event] == NULL) {
    return false; // unknown or unhandled: reported, not crashed
  }
  handlers[event](payload); // (*handlers[event])(payload) says the same
  return true;
}

// --- test instrumentation ----------------------------------------------------

static uint8_t seen_events[8];
static uint8_t seen_payloads[8];
static size_t seen_count;

static void record_tick(uint8_t payload) {
  seen_events[seen_count] = EVENT_TICK;
  seen_payloads[seen_count] = payload;
  ++seen_count;
}

static void record_uart(uint8_t payload) {
  seen_events[seen_count] = EVENT_UART;
  seen_payloads[seen_count] = payload;
  ++seen_count;
}

TEST("registered handlers run, in dispatch order") {
  CHECK(event_register(EVENT_TICK, record_tick));
  CHECK(event_register(EVENT_UART, record_uart));

  CHECK(event_dispatch(EVENT_UART, 0x55));
  CHECK(event_dispatch(EVENT_TICK, 7));

  REQUIRE(seen_count == 2);
  CHECK_EQ(seen_events[0], EVENT_UART);
  CHECK_EQ(seen_payloads[0], 0x55u);
  CHECK_EQ(seen_events[1], EVENT_TICK);
  CHECK_EQ(seen_payloads[1], 7u);
}

TEST("registration rejects nonsense instead of writing past the table") {
  CHECK_FALSE(event_register(200, record_tick));
  CHECK_FALSE(event_register(EVENT_COUNT, record_tick)); // first bad index
  CHECK_FALSE(event_register(EVENT_BUTTON, NULL));
}

TEST("dispatch with no handler reports false, and does not crash") {
  CHECK_FALSE(event_dispatch(EVENT_BUTTON, 0)); // nothing registered here
  CHECK_FALSE(event_dispatch(99, 0));           // no such event
}
