// Solution -- 08.04 The object pool: a free list that costs nothing

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

struct timer_event {
  uint32_t deadline;
  uint32_t payload;
};

// Each slot is EITHER a live event OR a link in the free list -- never both
// at once, which is exactly what a union says (05.04). The free list
// therefore occupies no memory of its own: it lives inside the objects
// that are not in use.

enum { POOL_SIZE = 4 };

union slot {
  struct timer_event event;
  union slot *next_free;
};

static union slot pool[POOL_SIZE];
static union slot *free_head;

void pool_init(void) {
  // Thread every slot onto the list, each pointing at the NEXT one.
  for (size_t i = 0; i + 1 < POOL_SIZE; ++i) {
    pool[i].next_free = &pool[i + 1];
  }
  pool[POOL_SIZE - 1].next_free = NULL;
  free_head = &pool[0];
}

struct timer_event *pool_alloc(void) {
  if (free_head == NULL) {
    return NULL; // exhaustion is an answer, not an accident
  }
  union slot *s = free_head;
  free_head = s->next_free;
  return &s->event;
}

void pool_release(struct timer_event *e) {
  // Recover the slot from the object pointer. The event is the union's
  // first member, so the addresses coincide; index arithmetic keeps the
  // compiler entirely out of aliasing arguments.
  const size_t index = (size_t)((uintptr_t)e - (uintptr_t)pool) / sizeof pool[0];
  union slot *s = &pool[index];
  // Push on the front: BOTH halves, the link and the head. Writing the
  // link without moving the head releases nothing.
  s->next_free = free_head;
  free_head = s;
}

TEST("the pool hands out its whole capacity, each object distinct") {
  pool_init();
  struct timer_event *events[POOL_SIZE];
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    events[i] = pool_alloc();
    REQUIRE(events[i] != NULL);
    events[i]->deadline = (uint32_t)(i * 100u);
  }
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    CHECK_EQ(events[i]->deadline, (uint32_t)(i * 100u));
  }
  CHECK(pool_alloc() == NULL); // the fifth object does not exist
}

TEST("a released object is reusable, most recent first") {
  pool_init();
  struct timer_event *a = pool_alloc();
  REQUIRE(a != NULL);
  pool_release(a);
  struct timer_event *b = pool_alloc();
  CHECK(b == a); // LIFO: the free list is a stack, and that is a feature --
                 // the hottest cache lines come back first
}

TEST("release all, allocate all: the pool does not shrink") {
  pool_init();
  struct timer_event *events[POOL_SIZE];
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    events[i] = pool_alloc();
    REQUIRE(events[i] != NULL);
  }
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    pool_release(events[i]);
  }
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    struct timer_event *e = pool_alloc();
    CHECK(e != NULL);
  }
  CHECK(pool_alloc() == NULL);
}
