// Solution -- 08.05 The arena: allocate forward, free everything at once

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum { ARENA_SIZE = 256 };

// _Alignas pins the storage to an 8-byte boundary, so an 8-aligned OFFSET
// into it yields an 8-aligned ADDRESS. Without it the compiler owes a byte
// array alignment 1 and every calculation below would be built on sand.
static struct {
  _Alignas(8) uint8_t bytes[ARENA_SIZE];
  uint8_t canary; // must survive everything this file does (00.03's trick)
} arena = {.canary = 0x5A};

static size_t arena_used;

void arena_reset(void) {
  arena_used = 0; // freeing EVERYTHING is one store -- that is the feature
}

size_t arena_available(void) {
  return (size_t)ARENA_SIZE - arena_used;
}

void *arena_alloc(size_t size, size_t align) {
  // Alignment must be a power of two (and no more than the storage's own
  // 8), or the mask arithmetic below is meaningless.
  if (align == 0 || align > 8 || (align & (align - 1)) != 0) {
    return NULL;
  }
  // Round the offset UP to the next multiple of align: add align-1, then
  // clear the low bits. The classic idiom -- 02.05 built the mask.
  const size_t aligned = (arena_used + (align - 1)) & ~(align - 1);
  // Check BEFORE moving anything, in the order that cannot overflow
  // (02.03): `aligned + size > ARENA_SIZE` could wrap; this cannot.
  if (aligned > ARENA_SIZE || size > (size_t)ARENA_SIZE - aligned) {
    return NULL;
  }
  void *p = &arena.bytes[aligned];
  arena_used = aligned + size;
  return p;
}

TEST("allocations come back aligned as asked") {
  arena_reset();
  CHECK(arena_alloc(1, 1) != NULL); // knock the offset off alignment
  uint32_t *p4 = arena_alloc(sizeof *p4, 4);
  REQUIRE(p4 != NULL);
  CHECK_EQ((uintptr_t)p4 & 3u, 0u);
  *p4 = 0xDEADBEEFu; // and the aligned pointer is genuinely usable
  CHECK(arena_alloc(1, 1) != NULL);
  uint64_t *p8 = arena_alloc(sizeof *p8, 8);
  REQUIRE(p8 != NULL);
  CHECK_EQ((uintptr_t)p8 & 7u, 0u);
  CHECK_EQ(*p4, 0xDEADBEEFu);
}

TEST("the arena refuses what does not fit -- and fills to the last byte") {
  arena_reset();
  CHECK(arena_alloc(200, 1) != NULL);
  CHECK(arena_alloc(100, 1) == NULL); // 200 + 100 > 256: refuse, do not lie
  CHECK(arena_alloc(56, 1) != NULL);  // 200 + 56 == 256: exactly fits
  CHECK_EQ(arena_available(), 0u);
  CHECK(arena_alloc(1, 1) == NULL);
}

TEST("distinct allocations do not overlap") {
  arena_reset();
  uint8_t *a = arena_alloc(16, 4);
  uint8_t *b = arena_alloc(16, 4);
  REQUIRE(a != NULL);
  REQUIRE(b != NULL);
  memset(a, 0x11, 16);
  memset(b, 0x22, 16);
  CHECK_EQ(a[0], 0x11u);
  CHECK_EQ(a[15], 0x11u); // b's writes must not have reached a
}

TEST("reset reclaims everything, and the canary lives") {
  arena_reset();
  uint8_t *p = arena_alloc(ARENA_SIZE, 1);
  REQUIRE(p != NULL);
  memset(p, 0xAA, ARENA_SIZE); // write every byte the arena granted
  CHECK_EQ(arena.canary, 0x5Au);
  arena_reset();
  CHECK_EQ(arena_available(), (size_t)ARENA_SIZE);
  CHECK(arena_alloc(ARENA_SIZE, 8) != NULL);
}
