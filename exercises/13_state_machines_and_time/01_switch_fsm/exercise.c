// =============================================================================
//  13.01 -- The switch-statement state machine
// =============================================================================
//
//  Most firmware is, at heart, a set of state machines fed by a loop. The
//  difference between firmware you can trust and firmware you debug at 2 am
//  is rarely the algorithm -- it is whether the states and events are
//  EXPLICIT. An enum for the states, an enum for the events, one function
//  that maps (state, event) to (state, action), and nothing else mutating
//  the state from the side.
//
//  The explicit form has a discipline:
//
//   - EVERY (state, event) PAIR IS DECIDED. Most pairs mean "ignore" -- a
//     stray sync byte mid-transfer, a completion for a transfer we never
//     started. Deciding them means counting them, because "illegal" is not
//     "impossible": serial lines glitch, peers retransmit, testers fuzz.
//     An undecided pair is where the 2 am bug lives.
//
//   - THE OUTER SWITCH HAS NO default. Enumerate the states and -Wswitch
//     (part of -Wall) turns "I added a state and forgot a handler" into a
//     compile error. A `default:` would eat that diagnostic to save four
//     lines. (MISRA C requires a default clause in every switch; with
//     enum-typed switches this course sides with the compiler check
//     instead. Know the tension before an auditor finds it for you.)
//
//   - ACTIONS LIVE ON TRANSITIONS. The counters in `struct link` change
//     only inside link_handle, so a test -- or a post-mortem -- can account
//     for every one of them.
//
//  THE MACHINE AT HAND is a firmware-update link, the kind a bootloader
//  speaks over a UART:
//
//      IDLE --SYNC--> SYNCED --HEADER--> RECEIVING --PAYLOAD_DONE-->
//          FLASHING --FLASH_DONE--> IDLE
//
//  and EV_ERROR, from ANY state, abandons the transfer and returns to IDLE.
//  "Any state" includes FLASHING: a CRC mismatch discovered mid-write is
//  precisely when you must bail out, mark the image bad and start over --
//  not the moment to hope for the best.
//
//  The starter has two bugs of the kind code review exists for: a
//  transition its author never finished, and an "any state" rule that one
//  state quietly opted out of. The tests name both.
//
//  TASK
//    Fix link_handle so all four tests pass. Do not change the tests.
//
//  RUN IT
//    ./mec test 13_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

enum link_state {
  LINK_IDLE,
  LINK_SYNCED,
  LINK_RECEIVING,
  LINK_FLASHING,
};

enum link_event {
  EV_SYNC,
  EV_HEADER,
  EV_PAYLOAD_DONE,
  EV_FLASH_DONE,
  EV_ERROR,
};

struct link {
  enum link_state state;
  unsigned flash_starts;   // times we began writing an image
  unsigned images_flashed; // times an image completed
  unsigned errors;         // EV_ERROR seen (from any state)
  unsigned ignored;        // events that had no meaning in their state
};

void link_init(struct link *l) {
  *l = (struct link){.state = LINK_IDLE};
}

void link_handle(struct link *l, enum link_event ev) {
  switch (l->state) {
  case LINK_IDLE:
    if (ev == EV_SYNC) {
      l->state = LINK_SYNCED;
    } else if (ev == EV_ERROR) {
      ++l->errors;
    } else {
      ++l->ignored;
    }
    break;

  case LINK_SYNCED:
    if (ev == EV_HEADER) {
      l->state = LINK_RECEIVING;
    } else if (ev == EV_ERROR) {
      ++l->errors;
      l->state = LINK_IDLE;
    } else {
      ++l->ignored;
    }
    break;

  case LINK_RECEIVING:
    // TODO: the transfer can never finish -- its author stopped here for
    // lunch and never came back. What should EV_PAYLOAD_DONE do?
    if (ev == EV_ERROR) {
      ++l->errors;
      l->state = LINK_IDLE;
    } else {
      ++l->ignored;
    }
    break;

  case LINK_FLASHING:
    // TODO: "surely nothing errors while we are writing flash" -- so this
    // state ignores EV_ERROR, violating the any-state rule. Read the
    // header for why mid-write is exactly when bailing out matters.
    if (ev == EV_FLASH_DONE) {
      ++l->images_flashed;
      l->state = LINK_IDLE;
    } else {
      ++l->ignored;
    }
    break;
  }
}

TEST("the happy path flashes an image and returns to idle") {
  struct link l;
  link_init(&l);

  link_handle(&l, EV_SYNC);
  CHECK_EQ((int)l.state, (int)LINK_SYNCED);
  link_handle(&l, EV_HEADER);
  CHECK_EQ((int)l.state, (int)LINK_RECEIVING);
  link_handle(&l, EV_PAYLOAD_DONE);
  CHECK_EQ((int)l.state, (int)LINK_FLASHING);
  CHECK_EQ(l.flash_starts, 1u);
  link_handle(&l, EV_FLASH_DONE);
  CHECK_EQ((int)l.state, (int)LINK_IDLE);
  CHECK_EQ(l.images_flashed, 1u);
  CHECK_EQ(l.errors, 0u);
  CHECK_EQ(l.ignored, 0u);
}

TEST("an error resets to idle from EVERY state") {
  const enum link_event path[] = {EV_SYNC, EV_HEADER, EV_PAYLOAD_DONE};

  // Walk 0, 1, 2 and 3 steps down the happy path, then inject EV_ERROR.
  for (unsigned depth = 0; depth <= 3; ++depth) {
    struct link l;
    link_init(&l);
    for (unsigned i = 0; i < depth; ++i) {
      link_handle(&l, path[i]);
    }
    link_handle(&l, EV_ERROR);
    CHECK_EQ((int)l.state, (int)LINK_IDLE);
    CHECK_EQ(l.errors, 1u);
  }
}

TEST("events with no meaning in a state are counted, not acted on") {
  struct link l;
  link_init(&l);

  link_handle(&l, EV_HEADER); // a header before sync is line noise
  CHECK_EQ((int)l.state, (int)LINK_IDLE);
  CHECK_EQ(l.ignored, 1u);

  link_handle(&l, EV_FLASH_DONE); // so is a completion we never started
  CHECK_EQ((int)l.state, (int)LINK_IDLE);
  CHECK_EQ(l.ignored, 2u);
}

TEST("a stray sync mid-transfer does not restart the machine") {
  struct link l;
  link_init(&l);
  link_handle(&l, EV_SYNC);
  link_handle(&l, EV_HEADER);

  link_handle(&l, EV_SYNC); // retransmitted sync byte: ignore it
  CHECK_EQ((int)l.state, (int)LINK_RECEIVING);
  CHECK_EQ(l.ignored, 1u);
}
