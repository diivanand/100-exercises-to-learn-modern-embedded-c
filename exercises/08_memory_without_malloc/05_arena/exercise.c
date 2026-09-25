// =============================================================================
//  08.05 -- The arena: allocate forward, free everything at once
// =============================================================================
//
//  The pool (08.04) serves many objects of ONE type. The arena serves the
//  other pattern: a burst of allocations of MIXED sizes with one common
//  lifetime -- everything a parser builds while handling one frame, gone
//  when the frame is answered (chapter 14 does exactly this).
//
//  The design is one line: keep an offset into a static buffer, bump it
//  forward for each allocation. Consequences:
//
//   - allocation is O(1) and branch-two: align, check, bump;
//   - there is NO per-allocation free -- arena_reset() frees everything at
//     once by setting the offset to zero. Not a limitation: for
//     one-lifetime data it is the exactly right amount of bookkeeping,
//     and an allocator with no free() cannot fragment (08.03) and cannot
//     double-free (08.02);
//   - anything that must OUTLIVE the reset must be copied out first. The
//     arena's discipline is the lifetime, not the type.
//
//  The subtlety is ALIGNMENT. arena_alloc(n, align) must return addresses
//  fit for the type being placed there: a uint32_t at an odd address is
//  undefined behaviour that a Cortex-M0 turns into a hard fault and a
//  Cortex-M4 quietly serves slowly (02.08). Two pieces make it work:
//
//   1. the STORAGE is pinned with _Alignas(8), so aligning the offset
//      aligns the address;
//   2. the offset is rounded up with the mask idiom
//      `(used + (align-1)) & ~(align-1)` -- 02.05's mask, earning rent.
//
//  And the bounds check must happen BEFORE the bump, phrased so it cannot
//  overflow (02.03): `size > ARENA_SIZE - aligned`, never
//  `aligned + size > ARENA_SIZE`.
//
//  TASK
//    arena_alloc validates `align` and then ignores it, and bumps the
//    offset without any bounds check at all -- it will happily "allocate"
//    memory past the end of the arena. Fix both. (The asan preset flags
//    the starter's misaligned store in the first test; run it.)
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 08_05
//
// =============================================================================

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
  if (align == 0 || align > 8 || (align & (align - 1)) != 0) {
    return NULL;
  }
  // TODO: `align` was checked and is then never used -- the offset goes
  // unrounded. And nothing checks that `size` still fits: the offset walks
  // straight past ARENA_SIZE and keeps "allocating".
  void *p = &arena.bytes[arena_used];
  arena_used += size;
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
