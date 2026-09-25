// Solution -- 13.02 The transition table

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
  ACT_IGNORE, // deliberately 0: see the header comment on zero-filled rows
  ACT_NONE,
  ACT_START_FLASH,
  ACT_FINISH,
  ACT_RESET,
};

struct transition {
  enum link_state next;
  enum link_action action;
};

// One row per state, one column per event, EVERY cell written out. The
// verbosity is the feature: a review can check this table against the
// protocol spec cell by cell, and nothing is left to what zero-fill
// happens to mean. The asserts pin the dimensions this table was written
// for; grow an enum and the file refuses to compile until you revisit it.
_Static_assert(LINK_STATE_COUNT == 4, "the table below covers 4 states");
_Static_assert(LINK_EVENT_COUNT == 5, "the table below covers 5 events");

static const struct transition table[LINK_STATE_COUNT][LINK_EVENT_COUNT] = {
    [LINK_IDLE] =
        {
            [EV_SYNC] = {LINK_SYNCED, ACT_NONE},
            [EV_HEADER] = {LINK_IDLE, ACT_IGNORE},
            [EV_PAYLOAD_DONE] = {LINK_IDLE, ACT_IGNORE},
            [EV_FLASH_DONE] = {LINK_IDLE, ACT_IGNORE},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
    [LINK_SYNCED] =
        {
            [EV_SYNC] = {LINK_SYNCED, ACT_IGNORE},
            [EV_HEADER] = {LINK_RECEIVING, ACT_NONE},
            [EV_PAYLOAD_DONE] = {LINK_SYNCED, ACT_IGNORE},
            [EV_FLASH_DONE] = {LINK_SYNCED, ACT_IGNORE},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
    [LINK_RECEIVING] =
        {
            [EV_SYNC] = {LINK_RECEIVING, ACT_IGNORE},
            [EV_HEADER] = {LINK_RECEIVING, ACT_IGNORE},
            [EV_PAYLOAD_DONE] = {LINK_FLASHING, ACT_START_FLASH},
            [EV_FLASH_DONE] = {LINK_RECEIVING, ACT_IGNORE},
            [EV_ERROR] = {LINK_IDLE, ACT_RESET},
        },
    [LINK_FLASHING] =
        {
            [EV_SYNC] = {LINK_FLASHING, ACT_IGNORE},
            [EV_HEADER] = {LINK_FLASHING, ACT_IGNORE},
            [EV_PAYLOAD_DONE] = {LINK_FLASHING, ACT_IGNORE},
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
                          (s == LINK_FLASHING && e == EV_FLASH_DONE) ||
                          (e == EV_ERROR);
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
