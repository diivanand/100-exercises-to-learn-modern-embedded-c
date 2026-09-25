// =============================================================================
//  03.07 -- void * context: C's answer to "whose callback is this?"
// =============================================================================
//
//  C has no closures and no generics. What it has is `void *` -- a pointer
//  that converts to and from ANY object pointer type, implicitly, no cast
//  needed (that is C; C++ made the conversion explicit, which is why you see
//  redundant casts in code that lived in both worlds). Paired with a
//  function pointer it gives you the CONTEXT idiom:
//
//      void register_cb(void (*cb)(void *ctx), void *ctx);
//
//  The caller hands over a callback AND a pointer to whatever state that
//  callback will need; the machinery stores both and hands the pointer back
//  on every invocation. The callback casts it back to the real type -- it
//  knows what it registered. Look at anything you will ever plug firmware
//  into and this pair looks back at you: FreeRTOS timers carry a pvTimerID,
//  every libc qsort_r/pthread_create takes a void * argument, and Zephyr,
//  libuv and the Linux kernel do the same under different parameter names.
//
//  Why not just use a file-scope global for the state? The starter does,
//  and the second test shows the cost: register a second instance and the
//  global is REBOUND -- last writer wins, and every earlier registration now
//  quietly feeds the newest context. One shared global means one instance,
//  ever. It also wrecks reentrancy (chapter 12 meets the same disease from
//  interrupts). The context pointer fixes both, because the state travels
//  WITH the registration instead of beside it.
//
//  Two disciplines make the idiom safe in practice:
//
//   - LIFETIME: the machinery stores the pointer, so the pointed-to object
//     must outlive the registration. Registering a soon-dead local is the
//     classic misuse (chapter 06 gives you the vocabulary for this).
//   - TYPE: the conversion back is unchecked, and converting to the WRONG
//     type is undefined behaviour with no diagnostic. The rule: the only
//     thing anyone may do with a ctx is convert it back to exactly what the
//     registering site put in.
//
//  TASK
//    Thread the context through `struct ticker`: store it at init, pass it
//    at fire. Then delete the global; nothing should need it. Do not change
//    the tests.
//
//  RUN IT
//    ./mec test 03_07
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>

typedef void (*ticker_cb)(void *ctx);

struct ticker {
  ticker_cb cb;
  // TODO: the context belongs in here.
};

// TODO: one global context, shared by every ticker -- last init wins.
static void *g_ctx;

void ticker_init(struct ticker *t, ticker_cb cb, void *ctx) {
  t->cb = cb;
  g_ctx = ctx;
}

void ticker_fire(struct ticker *t) {
  t->cb(g_ctx);
}

// --- test instrumentation ----------------------------------------------------

struct counter {
  unsigned ticks;
};

static void count_tick(void *ctx) {
  struct counter *c = ctx; // void * converts back implicitly: no cast in C
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

  CHECK_EQ(fast.ticks, 3u); // the starter feeds every fire to `slow`
  CHECK_EQ(slow.ticks, 1u);
}
