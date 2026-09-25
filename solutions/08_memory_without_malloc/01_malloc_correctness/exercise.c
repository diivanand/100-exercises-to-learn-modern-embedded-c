// Solution -- 08.01 malloc's contract, all four clauses

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
  // `sizeof *b`, not `sizeof(struct batch)`: it stays correct if the type
  // in the declaration ever changes, and it cannot name the WRONG type --
  // which is precisely the starter's first bug.
  struct batch *b = batch_alloc(sizeof *b);
  if (b == NULL) {
    return NULL;
  }
  // Refuse a count whose byte size does not exist. calloc does this check
  // for you (that is its entire advantage); with malloc it is yours to do.
  if (count > SIZE_MAX / sizeof *b->samples) {
    free(b);
    return NULL;
  }
  b->samples = batch_alloc(count * sizeof *b->samples);
  if (b->samples == NULL) {
    free(b); // the half-built object must not leak on the failure path
    return NULL;
  }
  b->count = count;
  return b;
}

void batch_destroy(struct batch *b) {
  // free(NULL) is defined to do nothing; destroy(NULL) should match.
  if (b == NULL) {
    return;
  }
  free(b->samples); // members first, then their owner -- never the reverse
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
