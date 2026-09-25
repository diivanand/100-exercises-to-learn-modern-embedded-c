// =============================================================================
//  03.06 -- Function pointers: the dispatch table
// =============================================================================
//
//  A function pointer stores WHICH code to run as data. Embedded systems are
//  built out of them: the vector table your Cortex-M boots from is an array
//  of function pointers at address zero, every RTOS callback is one, and
//  chapter 13 will build state machines from them. The syntax is the only
//  hard part, and a typedef removes it:
//
//      typedef void (*event_handler)(uint8_t payload);
//
//      event_handler h = record_tick;   // a function name decays, like an
//      h(7);                            // array name, to a pointer to it
//
//  Given the typedef, a DISPATCH TABLE is just an array:
//
//      static event_handler handlers[EVENT_COUNT];
//
//  and "handle event e" is one indexed call instead of a switch that grows a
//  case per event. The table costs a few pointers of RAM and buys you
//  runtime registration -- exactly the trade the interrupt controller makes.
//
//  A table of pointers has two failure modes, and the starter has both:
//
//  1. AN UNCHECKED INDEX. `handlers[200] = h` writes 197 slots past the
//     table, over whatever the linker placed next (the tests can only see
//     the lie in the return value; the sanitizer sees the write itself).
//     Bounds-check against EVENT_COUNT -- note the trailing-counter idiom in
//     the enum: add an event above it and every check stays right.
//
//  2. A NULL ENTRY CALLED. A slot nobody registered is NULL, and calling it
//     is the same crash as any other NULL dereference (CERT EXP34-C) -- on
//     the Cortex-M it is a jump through address zero, which lands in the
//     vector table and ends in a fault handler if you are lucky. Check, and
//     turn "no handler" into an honest `false`.
//
//  Policy question worth a sentence: should dispatch-with-no-handler be an
//  error, a silent no-op, or a default handler? All three exist in real
//  firmware; what matters is that it is a DECISION, not an accident. The
//  tests below specify `false`.
//
//  TASK
//    Add the bounds and NULL checks to both functions. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 03_06
//
// =============================================================================

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
  // TODO: no bounds check, no NULL check -- and it always claims success.
  handlers[event] = h;
  return true;
}

bool event_dispatch(uint8_t event, uint8_t payload) {
  // TODO: an unregistered slot is NULL, and this calls it anyway.
  handlers[event](payload);
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
