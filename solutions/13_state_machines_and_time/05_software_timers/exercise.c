// Solution -- 13.05 Software timers: many deadlines, one tick

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*timer_cb)(void *ctx);

enum { TIMER_SLOTS = 4 };

struct sw_timer {
  bool active;
  bool periodic;
  uint32_t due;    // when to fire next (wraparound-safe comparisons only)
  uint32_t period; // 0 for one-shots
  timer_cb cb;
  void *ctx;
};

struct timer_bank {
  struct sw_timer slot[TIMER_SLOTS];
};

void timers_init(struct timer_bank *b) {
  *b = (struct timer_bank){0};
}

static int arm(struct timer_bank *b, uint32_t due, uint32_t period,
               bool periodic, timer_cb cb, void *ctx) {
  for (int i = 0; i < TIMER_SLOTS; ++i) {
    if (!b->slot[i].active) {
      b->slot[i] = (struct sw_timer){
          .active = true,
          .periodic = periodic,
          .due = due,
          .period = period,
          .cb = cb,
          .ctx = ctx,
      };
      return i;
    }
  }
  return -1; // full: the caller decides whether that is fatal (09.01)
}

int timers_arm_oneshot(struct timer_bank *b, uint32_t now, uint32_t delay,
                       timer_cb cb, void *ctx) {
  return arm(b, now + delay, 0, false, cb, ctx);
}

int timers_arm_periodic(struct timer_bank *b, uint32_t now, uint32_t period,
                        timer_cb cb, void *ctx) {
  return arm(b, now + period, period, true, cb, ctx);
}

void timers_cancel(struct timer_bank *b, int id) {
  if (id >= 0 && id < TIMER_SLOTS) {
    b->slot[id].active = false;
  }
}

void timers_dispatch(struct timer_bank *b, uint32_t now) {
  for (int i = 0; i < TIMER_SLOTS; ++i) {
    struct sw_timer *t = &b->slot[i];
    if (!t->active) {
      continue;
    }
    if (now - t->due >= 0x80000000u) {
      continue; // not yet due (13.04's modular comparison)
    }

    // Bookkeeping BEFORE the callback, for two reasons that both bite:
    //  - a one-shot must free its slot first, so the callback may re-arm
    //    (a retry, a chained step) into the very slot that is firing;
    //  - a periodic re-arms FROM ITS DUE TIME, not from `now`. Dispatch
    //    always runs a little late; rescheduling from `now` bakes that
    //    lateness into every subsequent period and the timer drifts.
    if (t->periodic) {
      t->due += t->period;
    } else {
      t->active = false;
    }
    t->cb(t->ctx);
  }
}

// --- tests --------------------------------------------------------------------

struct counter {
  unsigned fires;
};

static void count_fire(void *ctx) {
  struct counter *c = ctx;
  ++c->fires;
}

TEST("a one-shot fires once, at or after its deadline") {
  struct timer_bank bank;
  timers_init(&bank);
  struct counter c = {0};

  const int id = timers_arm_oneshot(&bank, 0, 10, count_fire, &c);
  REQUIRE(id >= 0);

  timers_dispatch(&bank, 5);
  CHECK_EQ(c.fires, 0u);
  timers_dispatch(&bank, 10);
  CHECK_EQ(c.fires, 1u);
  timers_dispatch(&bank, 20); // long dead: must not fire again
  CHECK_EQ(c.fires, 1u);
}

TEST("slots exhaust, cancel frees, bad ids are harmless") {
  struct timer_bank bank;
  timers_init(&bank);
  struct counter c = {0};

  int ids[TIMER_SLOTS];
  for (int i = 0; i < TIMER_SLOTS; ++i) {
    ids[i] = timers_arm_oneshot(&bank, 0, 1000, count_fire, &c);
    CHECK(ids[i] >= 0);
  }
  CHECK_EQ(timers_arm_oneshot(&bank, 0, 1000, count_fire, &c), -1);

  timers_cancel(&bank, ids[2]);
  CHECK(timers_arm_oneshot(&bank, 0, 1000, count_fire, &c) >= 0);

  timers_cancel(&bank, -1); // out-of-range ids must be ignored,
  timers_cancel(&bank, 99); // not memory-corrupting
  CHECK_EQ(c.fires, 0u);
}

TEST("a periodic timer keeps the beat under dispatch jitter") {
  struct timer_bank bank;
  timers_init(&bank);
  struct counter c = {0};

  REQUIRE(timers_arm_periodic(&bank, 0, 10, count_fire, &c) >= 0);

  // A superloop never polls exactly on time. Dispatch at these instants:
  //
  //   now : 10  21  30  41  50  61  70  81  90  101
  //
  // Re-armed from its DUE time, the timer's schedule stays 10,20,30,...:
  //
  //   due : 10->20 20->30 30->40 40->50 50->60 60->70 70->80 80->90
  //         90->100 100->110            => fires at every dispatch: 10
  //
  // Re-armed from `now` (the starter), each late dispatch pushes the whole
  // schedule back: due 10,31,51,71,91,111 -- only 6 fires. Over a day that
  // "1 ms late sometimes" timer has lost forty seconds.
  const uint32_t script[] = {10, 21, 30, 41, 50, 61, 70, 81, 90, 101};
  for (size_t i = 0; i < sizeof script / sizeof script[0]; ++i) {
    timers_dispatch(&bank, script[i]);
  }
  CHECK_EQ(c.fires, 10u);
}

struct chain {
  struct timer_bank *bank;
  uint32_t now;      // the test keeps this current before each dispatch
  unsigned fires;
  bool arm_failed;
};

static void chain_fire(void *ctx) {
  struct chain *ch = ctx;
  ++ch->fires;
  if (ch->fires < 3) {
    // Re-arm from inside the callback: a retry chain. The slot that is
    // firing must already be free, or -- with every other slot occupied --
    // there is nowhere for this to go.
    if (timers_arm_oneshot(ch->bank, ch->now, 10, chain_fire, ch) < 0) {
      ch->arm_failed = true;
    }
  }
}

TEST("a callback can re-arm into the slot that is firing") {
  struct timer_bank bank;
  timers_init(&bank);
  struct counter dormant = {0};

  // Occupy every slot but one with far-future timers, so the chain's only
  // hope is the slot it itself is firing from.
  for (int i = 0; i < TIMER_SLOTS - 1; ++i) {
    REQUIRE(timers_arm_oneshot(&bank, 0, 1000000, count_fire, &dormant) >= 0);
  }

  struct chain ch = {.bank = &bank};
  REQUIRE(timers_arm_oneshot(&bank, 0, 10, chain_fire, &ch) >= 0);

  for (uint32_t now = 10; now <= 30; now += 10) {
    ch.now = now;
    timers_dispatch(&bank, now);
  }

  CHECK_FALSE(ch.arm_failed);
  CHECK_EQ(ch.fires, 3u);
  CHECK_EQ(dormant.fires, 0u); // the bystanders never fired
}
