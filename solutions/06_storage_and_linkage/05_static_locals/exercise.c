// Solution -- 06.05 Static locals: one instance per program, not per user

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

enum { WINDOW = 4 };

// The state became a value the CALLER owns. Each channel gets its own; an
// interrupt handler could have its own; a test can make a fresh one whenever
// it likes. This is the "objects in C" move: struct + functions taking a
// pointer to it, and it is how every serious C library is shaped.
struct smoother {
  uint16_t window[WINDOW];
  size_t next;
  size_t filled;
};

void smoother_init(struct smoother *s) {
  *s = (struct smoother){0};
}

uint16_t smoother_update(struct smoother *s, uint16_t sample) {
  s->window[s->next] = sample;
  s->next = (s->next + 1) % WINDOW;
  if (s->filled < WINDOW) {
    ++s->filled;
  }
  uint32_t sum = 0;
  for (size_t i = 0; i < s->filled; ++i) {
    sum += s->window[i];
  }
  return (uint16_t)(sum / s->filled);
}

TEST("a steady signal converges to its value") {
  struct smoother a;
  smoother_init(&a);
  uint16_t out = 0;
  for (int i = 0; i < 8; ++i) {
    out = smoother_update(&a, 400);
  }
  CHECK_EQ(out, 400u);
}

TEST("two channels do not bleed into each other") {
  struct smoother a;
  struct smoother b;
  smoother_init(&a);
  smoother_init(&b);
  for (int i = 0; i < 4; ++i) {
    smoother_update(&a, 400); // channel A: steady 400
    smoother_update(&b, 100); // channel B: steady 100
  }
  CHECK_EQ(smoother_update(&a, 400), 400u);
  CHECK_EQ(smoother_update(&b, 100), 100u);
}

TEST("a fresh smoother starts from scratch") {
  struct smoother a;
  smoother_init(&a);
  CHECK_EQ(smoother_update(&a, 500), 500u); // average of one sample
}
