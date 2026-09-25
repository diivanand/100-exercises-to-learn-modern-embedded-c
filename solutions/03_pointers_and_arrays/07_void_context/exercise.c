// Solution -- 03.07 void * context: C's answer to "whose callback is this?"

#include <mect/mect.h>

#include <stddef.h>

typedef void (*ticker_cb)(void *ctx);

struct ticker {
  ticker_cb cb;
  void *ctx; // the context RIDES WITH the registration -- per instance,
             // not in a file-scope global that every instance fights over
};

void ticker_init(struct ticker *t, ticker_cb cb, void *ctx) {
  t->cb = cb;
  t->ctx = ctx;
}

void ticker_fire(struct ticker *t) {
  t->cb(t->ctx); // hand back exactly what was registered
}

// --- test instrumentation ----------------------------------------------------

struct counter {
  unsigned ticks;
};

static void count_tick(void *ctx) {
  // void * converts to and from any object pointer implicitly in C -- no
  // cast required (writing one is C++ style and hides mistakes). The
  // assignment is the whole "generics" mechanism: the callback knows what
  // it registered, and takes it back.
  struct counter *c = ctx;
  c->ticks++;
}

TEST("a callback receives its own context") {
  struct counter c = {0};
  struct ticker t;
  ticker_init(&t, count_tick, &c);
  ticker_fire(&t);
  ticker_fire(&t);
  CHECK_EQ(c.ticks, 2u);
}

TEST("two instances do not share state") {
  struct counter fast = {0};
  struct counter slow = {0};
  struct ticker t_fast;
  struct ticker t_slow;
  ticker_init(&t_fast, count_tick, &fast);
  ticker_init(&t_slow, count_tick, &slow);

  ticker_fire(&t_fast);
  ticker_fire(&t_fast);
  ticker_fire(&t_fast);
  ticker_fire(&t_slow);

  CHECK_EQ(fast.ticks, 3u);
  CHECK_EQ(slow.ticks, 1u);
}
