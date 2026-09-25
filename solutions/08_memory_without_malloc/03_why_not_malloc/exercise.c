// Solution -- 08.03 Why firmware says no to the heap

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// A toy heap: sixteen slots, allocations are contiguous runs. Real
// allocators manage bytes and block headers, but every pathology this model
// shows -- fragmentation above all -- they show too, only less legibly.

enum { HEAP_SLOTS = 16 };
static bool slot_used[HEAP_SLOTS];

void fake_heap_reset(void) {
  memset(slot_used, 0, sizeof slot_used);
}

// First-fit: return the start of the first free run of `len` slots, marking
// it used; -1 if no such run exists (however much TOTAL space is free).
int fake_alloc(size_t len) {
  if (len == 0 || len > HEAP_SLOTS) {
    return -1;
  }
  size_t run = 0;
  for (size_t i = 0; i < HEAP_SLOTS; ++i) {
    run = slot_used[i] ? 0 : run + 1;
    if (run == len) {
      const size_t start = i + 1 - len;
      for (size_t j = start; j <= i; ++j) {
        slot_used[j] = true;
      }
      return (int)start;
    }
  }
  return -1;
}

void fake_free(int start, size_t len) {
  // Every slot back, not len - 1 of them: an allocator that returns less
  // than it was given leaks by a thousand cuts.
  for (size_t i = 0; i < len; ++i) {
    slot_used[(size_t)start + i] = false;
  }
}

size_t largest_free_run(void) {
  // The number that actually decides whether an allocation succeeds. It is
  // the RUN that matters, and the run must reset at every used slot.
  size_t run = 0;
  size_t best = 0;
  for (size_t i = 0; i < HEAP_SLOTS; ++i) {
    run = slot_used[i] ? 0 : run + 1;
    if (run > best) {
      best = run;
    }
  }
  return best;
}

size_t total_free(void) {
  size_t n = 0;
  for (size_t i = 0; i < HEAP_SLOTS; ++i) {
    if (!slot_used[i]) {
      ++n;
    }
  }
  return n;
}

TEST("freed space is reusable, first-fit from the bottom") {
  fake_heap_reset();
  CHECK_EQ(fake_alloc(4), 0);
  CHECK_EQ(fake_alloc(4), 4);
  fake_free(0, 4);
  CHECK_EQ(fake_alloc(4), 0); // the hole at 0 fits, so first-fit takes it
  CHECK_EQ(total_free(), 8u);
}

TEST("fragmentation: enough free space, none of it usable") {
  fake_heap_reset();
  CHECK_EQ(fake_alloc(4), 0);  // a
  CHECK_EQ(fake_alloc(4), 4);  // b
  CHECK_EQ(fake_alloc(4), 8);  // c
  CHECK_EQ(fake_alloc(4), 12); // d
  fake_free(4, 4);             // free b ...
  fake_free(12, 4);            // ... and d: two separate 4-slot holes
  CHECK_EQ(total_free(), 8u);
  CHECK_EQ(largest_free_run(), 4u);
  // Eight slots are free and this request for six of them FAILS. That is
  // fragmentation, and no amount of correct code on the caller's side can
  // prevent it -- only the allocation PATTERN decides.
  CHECK_EQ(fake_alloc(6), -1);
  CHECK_EQ(fake_alloc(3), 4); // small requests still fit in the first hole
}

TEST("exhaustion is reported, not hidden") {
  fake_heap_reset();
  CHECK_EQ(fake_alloc(16), 0);
  CHECK_EQ(fake_alloc(1), -1);
  CHECK_EQ(total_free(), 0u);
}
