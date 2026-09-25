// =============================================================================
//  01.03 -- Initialisation: garbage, zeros, and who provides them
// =============================================================================
//
//  Three storage classes, three different answers to "what does a variable
//  hold before you write to it?":
//
//   - AUTOMATIC (locals): garbage. Whatever the stack held last. Reading it
//     is undefined behaviour (CERT EXP33-C), and it is the worst kind of
//     bug: on the bench the stack happens to hold zeros and everything
//     works; in the field, after a different call path, it does not.
//   - STATIC (globals, and locals marked `static`): zero, guaranteed by the
//     language before main() runs. On the target, that zero is not free --
//     06.04 shows the startup loop that writes it, and 15.02 makes you write
//     that loop yourself.
//   - ALLOCATED (malloc): garbage again -- chapter 08's problem.
//
//  For aggregates there is one initialiser worth memorising:
//
//      struct stats s = {0};                  // every member zero
//      *s = (struct stats){.min = UINT32_MAX};  // named ones set, REST ZEROED
//
//  The rule doing the work (C17 6.7.9p19): members a braced initialiser does
//  not name are initialised as static objects would be -- zero. That makes
//  whole-object assignment the reset idiom that survives growth: name the
//  members with non-zero defaults and let the language zero the rest.
//
//  The starter below instead initialises members ONE BY ONE -- and the
//  struct has grown since that code was written. `min` and `max` were added
//  for a diagnostics page; nobody revisited stats_init. There is also a
//  sequence-number generator whose author wanted a counter that survives
//  calls but reset it on every one.
//
//  (The tests memset the struct to 0xAB first. That is what "garbage" looks
//  like when you want it reproducible -- a cold stack is not obliged to be
//  so kind.)
//
//  TASK
//    Fix stats_init with a whole-struct assignment, and make the sequence
//    counter survive between calls. Do not change the tests.
//
//  RUN IT
//    ./mec test 01_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>
#include <string.h>

struct stats {
  uint32_t count;
  uint32_t sum;
  uint32_t min; // added later: smallest sample seen, UINT32_MAX when empty
  uint32_t max; // added later: largest sample seen, 0 when empty
};

void stats_init(struct stats *s) {
  // TODO: this list was complete once. The struct has grown since.
  s->count = 0;
  s->sum = 0;
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
  // TODO: this counter is born again, aged zero, on every call.
  uint32_t n = 0;
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
