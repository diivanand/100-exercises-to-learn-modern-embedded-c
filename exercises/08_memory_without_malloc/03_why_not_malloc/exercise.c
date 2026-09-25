// =============================================================================
//  08.03 -- Why firmware says no to the heap
// =============================================================================
//
//  MISRA C Directive 4.12: "Dynamic memory allocation shall not be used."
//  Not "used carefully" -- not used. Four reasons, in rising order of
//  severity:
//
//   1. TIMING. malloc walks a free list; its worst case is unbounded and
//      depends on everything that ever happened to the heap. An ISR-fed
//      control loop cannot budget for "usually fast".
//
//   2. OVERHEAD. Every heap block carries a header (8-16 bytes on a 32-bit
//      allocator). If your objects are 16-byte messages, HALF your RAM is
//      bookkeeping. The pool in 08.04 spends zero.
//
//   3. FAILURE HANDLING. Every call site inherits a NULL branch that must
//      do something sensible on a device with nobody watching (08.01). A
//      static buffer cannot fail at 2 a.m.; it failed at link time or never
//      (see _min_heap_size in bsp/stm32l476rg.ld for the link-time version).
//
//   4. FRAGMENTATION -- the killer. Free space stops being USABLE space:
//      after months of mixed-size allocate/free traffic, the heap has
//      plenty of bytes free and no RUN long enough for the request in
//      hand. No code review catches it, because it is not a bug in any
//      line; it is an emergent property of the allocation pattern. Devices
//      that "need a reboot every few weeks" are very often this.
//
//  This exercise makes fragmentation visible at toy scale: a sixteen-slot
//  heap, allocations as contiguous runs, first-fit. When your allocator
//  works, the second test walks it into the classic corner -- eight slots
//  free, a six-slot request refused -- and ASSERTS the refusal. The test
//  passes when fragmentation happens, because fragmentation is not a
//  defect in the allocator: it is the allocator working as designed.
//
//  Where malloc IS fine: allocate-at-boot-and-never-free (a heap that
//  never frees cannot fragment), and embedded Linux, where an MMU, swap
//  and an OOM killer change the economics -- there the rules are 08.01's.
//  (Effective C ch. 6 "Safety-Critical Systems"; Grenning, TDD for
//  Embedded C ch. 6, "We Have Constrained Memory".)
//
//  TASK
//    fake_free returns one slot too few, and largest_free_run forgets
//    that a RUN ends at a used slot. Fix both. Do not change the tests.
//
//  RUN IT
//    ./mec test 08_03
//
// =============================================================================

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
  // TODO: one slot short. An allocator that gives back less than it was
  // given leaks by a thousand cuts.
  for (size_t i = 0; i + 1 < len; ++i) {
    slot_used[(size_t)start + i] = false;
  }
}

size_t largest_free_run(void) {
  // TODO: this counts every free slot it meets, which is total_free by
  // another name. A RUN ends at the first used slot.
  size_t run = 0;
  for (size_t i = 0; i < HEAP_SLOTS; ++i) {
    if (!slot_used[i]) {
      ++run;
    }
  }
  return run;
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
