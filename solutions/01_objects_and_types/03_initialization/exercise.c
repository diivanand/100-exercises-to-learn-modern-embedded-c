// Solution -- 01.03 Initialisation: garbage, zeros, and who provides them

#include <mect/mect.h>

#include <stdint.h>
#include <string.h>

struct stats {
  uint32_t count;
  uint32_t sum;
  uint32_t min;
  uint32_t max;
};

void stats_init(struct stats *s) {
  // One assignment covers EVERY member, present and future: the ones this
  // braced value names get their values, and C guarantees the rest are
  // zeroed (C17 6.7.9p19 -- the same rule that makes `= {0}` work). When a
  // colleague adds a field next year, this function is already correct.
  *s = (struct stats){.min = UINT32_MAX};
}

void stats_add(struct stats *s, uint32_t sample) {
  s->count += 1;
  s->sum += sample;
  if (sample < s->min) {
    s->min = sample;
  }
  if (sample > s->max) {
    s->max = sample;
  }
}

uint32_t next_sequence_number(void) {
  // `static` moves n to static storage: it exists once, for the life of the
  // program, and the language zero-initialises it before main() -- no
  // explicit `= 0` required (though writing one costs nothing). On the
  // target, 06.04 shows the startup code that actually delivers that zero.
  static uint32_t n;
  return ++n;
}

TEST("init establishes the empty-state invariants") {
  struct stats s;
  memset(&s, 0xAB, sizeof s); // simulate the garbage a cold stack hands you

  stats_init(&s);

  CHECK_EQ(s.count, 0u);
  CHECK_EQ(s.sum, 0u);
  CHECK_EQ(s.min, UINT32_MAX); // "no sample yet" must lose to any real sample
  CHECK_EQ(s.max, 0u);
}

TEST("min and max are honest after samples arrive") {
  struct stats s;
  memset(&s, 0xAB, sizeof s);
  stats_init(&s);

  stats_add(&s, 42);
  stats_add(&s, 7);
  stats_add(&s, 99);

  CHECK_EQ(s.count, 3u);
  CHECK_EQ(s.sum, 148u);
  CHECK_EQ(s.min, 7u);
  CHECK_EQ(s.max, 99u);
}

TEST("sequence numbers actually advance") {
  CHECK_EQ(next_sequence_number(), 1u);
  CHECK_EQ(next_sequence_number(), 2u);
  CHECK_EQ(next_sequence_number(), 3u);
}
