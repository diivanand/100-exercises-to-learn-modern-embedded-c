// =============================================================================
//  08.01 -- malloc's contract, all four clauses
// =============================================================================
//
//  This chapter is about living WITHOUT the heap, because that is how most
//  firmware lives: no malloc after boot, sometimes no malloc at all (MISRA
//  C Dir 4.12 bans it outright; 08.03 makes the case). But you cannot
//  retire a tool you cannot use. On embedded Linux malloc is everywhere,
//  and its contract has four clauses people break daily:
//
//   1. CHECK THE RETURN. malloc tells you it failed exactly once, by
//      returning NULL. Dereferencing that NULL is CERT MEM35-C's most
//      popular ending. On a long-running device, allocation failure is not
//      exotic -- it is Tuesday.
//
//   2. ASK FOR THE RIGHT SIZE. Write `p = malloc(sizeof *p)` and the size
//      is derived FROM the pointer being assigned -- it cannot name the
//      wrong type. `malloc(sizeof(struct batch *))` compiles, allocates
//      eight bytes for a sixteen-byte struct, and works at -O0 because the
//      allocator rounds up. The asan build sees through the slack: run
//      `cmake --preset asan` on this starter and read the report.
//
//   3. CHECK THE MULTIPLY. `count * sizeof(int32_t)` is size_t arithmetic:
//      it WRAPS (02.03). A count of SIZE_MAX/4 + 1 wraps the byte size to
//      zero, malloc happily returns a tiny allocation, and you now own a
//      "batch" of four billion samples backed by nothing. calloc(n, size)
//      exists because it performs this overflow check for you (Effective C
//      ch. 6); with malloc the check is yours. CERT MEM07-C.
//
//   4. FREE EXACTLY ONCE, MEMBERS FIRST. Freeing the owner before its
//      members orphans them (a leak); freeing twice corrupts the heap
//      (CERT MEM30-C, MEM31-C). And free(NULL) is defined to do nothing --
//      a destroy function should extend the same courtesy.
//
//  One more thing to notice: `batch_alloc` is a FUNCTION POINTER (03.06)
//  the tests can repoint. You cannot make the real malloc fail on cue, so
//  the test injects one that does. Chapter 10 turns this trick -- the seam
//  -- into a discipline.
//
//  TASK
//    batch_create and batch_destroy each break the contract. Fix them.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 08_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>
#include <stdlib.h>

struct batch {
  size_t count;
  int32_t *samples;
};

// The allocator seam: tests swap this to make allocation fail on demand.
static void *(*batch_alloc)(size_t) = malloc;

struct batch *batch_create(size_t count) {
  // TODO: this asks for the size of a POINTER, not of the struct; nothing
  // checks either allocation; and the multiply can wrap to a tiny number.
  struct batch *b = batch_alloc(sizeof(struct batch *));
  b->count = count;
  b->samples = batch_alloc(count * sizeof(int32_t));
  return b;
}

void batch_destroy(struct batch *b) {
  // TODO: destroy(NULL) should be as harmless as free(NULL).
  free(b->samples);
  free(b);
}

static void *failing_alloc(size_t size) {
  (void)size;
  return NULL; // what malloc does at 2 a.m. on a long-running device
}

TEST("a small batch works end to end") {
  struct batch *b = batch_create(8);
  REQUIRE(b != NULL);
  CHECK_EQ(b->count, 8u);
  for (size_t i = 0; i < b->count; ++i) {
    b->samples[i] = (int32_t)(i * 3u);
  }
  CHECK_EQ(b->samples[7], 21);
  batch_destroy(b);
}

TEST("a count whose byte size overflows is refused") {
  // count * sizeof(int32_t) wraps to 0 here: an unchecked multiply asks
  // malloc for 0 bytes and gets a tiny allocation posing as 2^62 samples.
  struct batch *b = batch_create(SIZE_MAX / sizeof(int32_t) + 1u);
  CHECK(b == NULL);
  batch_destroy(b);
}

TEST("allocation failure comes back as NULL, not as a crash") {
  batch_alloc = failing_alloc;
  struct batch *b = batch_create(4);
  CHECK(b == NULL);
  batch_alloc = malloc;
}

TEST("destroying NULL is allowed, like free(NULL)") {
  batch_destroy(NULL);
  CHECK(true); // reaching this line is the test
}
