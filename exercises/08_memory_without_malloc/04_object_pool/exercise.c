// =============================================================================
//  08.04 -- The object pool: a free list that costs nothing
// =============================================================================
//
//  08.03 retired malloc; this exercise and the next two are what firmware
//  uses instead. First: THE OBJECT POOL, for when you allocate and free
//  many objects of ONE type -- timer events, message descriptors,
//  connection slots.
//
//   - A fixed array of N slots decides the worst case AT LINK TIME. Sizing
//     the pool forces the design question malloc lets you dodge: how many
//     of these can exist at once, and what happens on the N+1th?
//   - Allocation and release are O(1), a pointer swap each. No scan, no
//     header, no fragmentation -- every slot is the same size, so any free
//     slot fits any request.
//   - The free list THREADS THROUGH THE FREE SLOTS THEMSELVES: a slot is
//     either a live object or a link, never both, so the list's memory
//     cost is zero. The union below says exactly that (05.04), and this
//     trick is in every RTOS's fixed-block allocator (Grenning, TDD for
//     Embedded C ch. 6, reaches the same design).
//
//  Exhaustion returns NULL, and that is not a failure of the pool -- it is
//  the pool doing its job: the caller decides whether to drop, retry or
//  assert, per 09.01's error discipline. What must NEVER happen is the
//  pool handing out a slot that is still in use; the first test writes
//  through every handle and reads them all back to catch exactly that.
//
//  TASK
//    pool_release performs half of a push: it writes the link but never
//    moves the head, so released slots are lost forever and the pool only
//    shrinks. Finish the push. Do not change the tests.
//
//  RUN IT
//    ./mec test 08_04
//
// =============================================================================

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
  // TODO: this is half of a push onto the free list.
  s->next_free = free_head;
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
