// Solution -- 13.01 The switch-statement state machine

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
  // EV_ERROR means the same thing everywhere: drop the transfer, go back
  // to idle. Handling it once, before the per-state switch, keeps the
  // "from any state" rule in one place instead of five.
  if (ev == EV_ERROR) {
    ++l->errors;
    l->state = LINK_IDLE;
    return;
  }

  // The outer switch enumerates every state and has NO default, so -Wswitch
  // (part of -Wall) reports at compile time any state a future edit adds
  // and forgets to handle. The inner defaults count the events that carry
  // no meaning in this state -- decided, logged, harmless.
  switch (l->state) {
  case LINK_IDLE:
    if (ev == EV_SYNC) {
      l->state = LINK_SYNCED;
    } else {
      ++l->ignored;
    }
    break;

  case LINK_SYNCED:
    if (ev == EV_HEADER) {
      l->state = LINK_RECEIVING;
    } else {
      ++l->ignored;
    }
    break;

  case LINK_RECEIVING:
    if (ev == EV_PAYLOAD_DONE) {
      ++l->flash_starts;
      l->state = LINK_FLASHING;
    } else {
      ++l->ignored;
    }
    break;

  case LINK_FLASHING:
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
