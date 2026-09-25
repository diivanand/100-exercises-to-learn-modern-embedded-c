// =============================================================================
//  13.02 -- The transition table
// =============================================================================
//
//  13.01's switch works, but the machine's SHAPE is smeared across eighty
//  lines of control flow. The alternative: make the machine DATA. One
//  two-dimensional table, indexed by state and event, each cell holding
//  (next state, action); the dispatch function shrinks to four lines that
//  never change again.
//
//      const struct transition *t = &table[state][event];
//      do(t->action);
//      state = t->next;
//
//  What the table form buys:
//
//   - REVIEWABILITY. The table is the protocol spec, transcribed. A
//     reviewer checks cells against the datasheet instead of tracing
//     branches.
//   - PROVABLE TOTALITY. A switch's coverage is checked by -Wswitch at
//     best; a table's dimensions are types. The _Static_asserts below pin
//     the table to exactly LINK_STATE_COUNT x LINK_EVENT_COUNT, so adding
//     a state will not compile until someone revisits this file.
//   - TOOLING. Tables can be generated -- from a spec, a DSL, a spreadsheet
//     -- and diffed. Nobody diffs control flow.
//
//  What it costs: guards and parameters fit badly (a transition that
//  depends on a payload value needs escape hatches), and each cell is as
//  dumb as a cell can be. Dense machines with simple transitions -- exactly
//  the protocol/bootloader kind -- are where tables win.
//
//  THE TRAP THIS STARTER IS SITTING IN. Designated initialisers zero-fill
//  whatever you leave out (01.04), and in this table a zero cell reads as
//
//      { .next = 0, .action = 0 }  ==  { LINK_IDLE, ACT_IGNORE }
//
//  -- which is not "do nothing". It is "count the event as ignored, then
//  TELEPORT TO IDLE", because state 0 happens to be LINK_IDLE. The starter
//  wrote only the "interesting" cells and let zero-fill mean ignore; the
//  third test sweeps every (state, event) pair and catches the machine
//  quietly resetting mid-transfer on a retransmitted sync byte. Write every
//  cell. The verbosity is the review surface.
//
//  (Same machine as 13.01: IDLE -> SYNCED -> RECEIVING -> FLASHING -> IDLE,
//  EV_ERROR resets from anywhere, everything else is counted and ignored.)
//
//  TASK
//    Complete the table so all three tests pass -- every cell explicit.
//    Do not change the tests or the dispatch function.
//
//  RUN IT
//    ./mec test 13_02
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

enum link_state {
  LINK_IDLE,
  LINK_SYNCED,
  LINK_RECEIVING,
  LINK_FLASHING,
  LINK_STATE_COUNT,
};

enum link_event {
  EV_SYNC,
  EV_HEADER,
  EV_PAYLOAD_DONE,
  EV_FLASH_DONE,
  EV_ERROR,
  LINK_EVENT_COUNT,
};

enum link_action {
  ACT_IGNORE, // deliberately 0: a zero-filled cell at least COUNTS itself
  ACT_NONE,
  ACT_START_FLASH,
  ACT_FINISH,
  ACT_RESET,
};

struct transition {
  enum link_state next;
  enum link_action action;
};

_Static_assert(LINK_STATE_COUNT == 4, "the table below covers 4 states");
_Static_assert(LINK_EVENT_COUNT == 5, "the table below covers 5 events");

// TODO: only the "interesting" cells are written; everything else is
// zero-filled, and zero means { LINK_IDLE, ACT_IGNORE } -- an ignore that
// teleports. Spell out every cell of every row.
static const struct transition table[LINK_STATE_COUNT][LINK_EVENT_COUNT] = {
    [LINK_IDLE] =
        {
            [EV_SYNC] = {LINK_SYNCED, ACT_NONE},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
    [LINK_SYNCED] =
        {
            [EV_HEADER] = {LINK_RECEIVING, ACT_NONE},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
    [LINK_RECEIVING] =
        {
            [EV_PAYLOAD_DONE] = {LINK_FLASHING, ACT_START_FLASH},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
    [LINK_FLASHING] =
        {
            [EV_FLASH_DONE] = {LINK_IDLE, ACT_FINISH},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
};

struct link {
  enum link_state state;
  unsigned flash_starts;
  unsigned images_flashed;
  unsigned errors;
  unsigned ignored;
};

void link_init(struct link *l) {
  *l = (struct link){.state = LINK_IDLE};
}

void link_handle(struct link *l, enum link_event ev) {
  const struct transition *t = &table[l->state][ev];
  switch (t->action) {
  case ACT_IGNORE:
    ++l->ignored;
    break;
  case ACT_NONE:
    break;
  case ACT_START_FLASH:
    ++l->flash_starts;
    break;
  case ACT_FINISH:
    ++l->images_flashed;
    break;
  case ACT_RESET:
    ++l->errors;
    break;
  }
  l->state = t->next;
}

TEST("the happy path flashes an image and returns to idle") {
  struct link l;
  link_init(&l);

  link_handle(&l, EV_SYNC);
  link_handle(&l, EV_HEADER);
  link_handle(&l, EV_PAYLOAD_DONE);
  CHECK_EQ((int)l.state, (int)LINK_FLASHING);
  CHECK_EQ(l.flash_starts, 1u);
  link_handle(&l, EV_FLASH_DONE);
  CHECK_EQ((int)l.state, (int)LINK_IDLE);
  CHECK_EQ(l.images_flashed, 1u);
  CHECK_EQ(l.ignored, 0u);
}

TEST("an error resets to idle from every state") {
  const enum link_event path[] = {EV_SYNC, EV_HEADER, EV_PAYLOAD_DONE};

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

// The totality sweep: for EVERY (state, event) pair, an event that is not
// one of the five real transitions must leave the state alone and count as
// ignored. This is the test that catches a zero-filled hole in the table --
// a hole does not "do nothing", it teleports the machine to state 0.
TEST("every undecided pair holds its state (no zero-fill teleports)") {
  const enum link_event path[] = {EV_SYNC, EV_HEADER, EV_PAYLOAD_DONE};

  for (unsigned s = 0; s < (unsigned)LINK_STATE_COUNT; ++s) {
    for (unsigned e = 0; e < (unsigned)LINK_EVENT_COUNT; ++e) {
      // The five real transitions of the protocol, spelled out as the
      // test's own oracle. Everything else must be a stay-put ignore.
      const int is_real = (s == LINK_IDLE && e == EV_SYNC) ||
                          (s == LINK_SYNCED && e == EV_HEADER) ||
                          (s == LINK_RECEIVING && e == EV_PAYLOAD_DONE) ||
                          (s == LINK_FLASHING && e == EV_FLASH_DONE) || (e == EV_ERROR);
      if (is_real) {
        continue;
      }

      struct link l;
      link_init(&l);
      for (unsigned i = 0; i < s; ++i) { // s doubles as path depth: state
        link_handle(&l, path[i]);        // N is N steps down the happy path
      }
      REQUIRE((unsigned)l.state == s);

      link_handle(&l, (enum link_event)e);
      CHECK_EQ((unsigned)l.state, s); // stayed put
      CHECK_EQ(l.ignored, 1u);        // and said so
    }
  }
}
