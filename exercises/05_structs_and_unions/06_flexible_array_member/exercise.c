// =============================================================================
//  05.06 -- Flexible array members: one struct, many sizes
// =============================================================================
//
//  A log wants entries of varying length: a fixed header (tick, severity,
//  length) and then THAT entry's text, nothing more. Three shapes compete:
//
//      char text[MAX];     // every entry pays for the longest possible one
//      char *text;         // pointing at... whose memory? a second
//                          // allocation, a second lifetime, a second failure
//      char text[];        // the C99 FLEXIBLE ARRAY MEMBER
//
//  The FAM (Effective C 2nd ed. ch. 6) is an array with no size, declared as
//  the LAST member. The struct ends where the array begins, and whoever
//  allocates the object decides how much text follows. Header and payload
//  stay contiguous: one allocation, one lifetime, no pointer to chase. (Its
//  pre-C99 ancestor, `char text[1]` plus out-of-bounds indexing, is
//  undefined behaviour -- CERT DCL38-C.)
//
//  The size arithmetic is the whole trick, and sizeof gets it wrong:
//
//      struct log_msg { uint32_t tick; uint8_t severity; uint8_t len;
//                       char text[]; };
//
//      offsetof(struct log_msg, text)  ==  6    the text really starts here
//      sizeof(struct log_msg)          ==  8    rounded up for array duty
//
//  sizeof includes the trailing padding an ARRAY of these would need (05.01)
//  -- but nobody makes arrays of these. An entry needs
//
//      offsetof(struct log_msg, text) + len
//
//  bytes, rounded back up so the NEXT entry's uint32_t is 4-aligned (CERT
//  MEM33-C covers sizing FAM structs). Entries here are carved from a static
//  byte arena -- no malloc on this course; chapter 08 says why -- each laid
//  at the current offset, the offset advanced by the entry's full aligned
//  size. The walker, log_get(), already strides exactly that way.
//
//  The starter's log_append() does not. It advances past the text it copied
//  and forgets the header (and the alignment) entirely, so the SECOND
//  entry's header is written into the FIRST entry's text. Watch the tests
//  read back a clobbered length and a tick of zero. Two things are worth
//  noticing about that failure: it is corruption by overlap, which is what
//  FAM size bugs look like in the field -- and AddressSanitizer cannot see
//  it, because every write lands inside the arena, which is one object as
//  far as asan is concerned. (The asan preset does still flag the starter:
//  UBSan objects to the second entry's misaligned stores -- a different
//  symptom of the same wrong offset.)
//
//  TASK
//    Fix the advance in `log_append` so entries are laid end to end by the
//    same arithmetic the walker uses. Do not change the tests.
//
//  RUN IT
//    ./mec test 05_06
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct log_msg {
  uint32_t tick;
  uint8_t severity;
  uint8_t len;
  char text[]; // flexible array member: always last, never the only member
};

_Static_assert(_Alignof(struct log_msg) == 4, "align_up_4 assumes this");

enum { LOG_ARENA_SIZE = 256 };

// The arena must be at least as aligned as the structs carved from it.
// (Carving typed objects out of a byte arena is what every allocator does;
// the discipline that keeps it sound in practice is accessing each carved
// region through ONE type only -- these bytes are only ever a log_msg.)
static _Alignas(struct log_msg) uint8_t log_arena[LOG_ARENA_SIZE];
static size_t log_arena_used;
static size_t log_msg_count;

// Round up to the struct's alignment, so every entry's uint32_t tick is
// addressable. Note ~(size_t)3, not ~3u: the complement must happen at
// size_t's width, or the mask would shear off the high bits -- 02.01's
// lesson, still collecting rent.
static size_t align_up_4(size_t n) {
  return (n + 3u) & ~(size_t)3;
}

// The bytes one entry occupies: header up to the text, the text itself,
// alignment for whoever comes next. offsetof, NOT sizeof -- see the header
// comment.
static size_t entry_size(uint8_t len) {
  return align_up_4(offsetof(struct log_msg, text) + len);
}

struct log_msg *log_append(uint32_t tick, uint8_t severity, const char *text) {
  const size_t n = strlen(text);
  if (n > UINT8_MAX) {
    return NULL;
  }
  const size_t need = entry_size((uint8_t)n);
  if (need > sizeof log_arena - log_arena_used) {
    return NULL; // full: the caller decides what dropping a message means
  }
  struct log_msg *m = (struct log_msg *)(void *)&log_arena[log_arena_used];
  m->tick = tick;
  m->severity = severity;
  m->len = (uint8_t)n;
  memcpy(m->text, text, n);
  // TODO: this advances past the text bytes alone. The header the next
  // entry needs -- and its alignment -- were left out, so the next append
  // writes straight into what this one just stored.
  log_arena_used += n;
  ++log_msg_count;
  return m;
}

const struct log_msg *log_get(size_t index) {
  if (index >= log_msg_count) {
    return NULL;
  }
  size_t off = 0;
  for (size_t i = 0; i < index; ++i) {
    const struct log_msg *m = (const struct log_msg *)(const void *)&log_arena[off];
    off += entry_size(m->len);
  }
  return (const struct log_msg *)(const void *)&log_arena[off];
}

size_t log_count(void) {
  return log_msg_count;
}

void log_reset(void) {
  log_arena_used = 0;
  log_msg_count = 0;
}

TEST("sizeof does not count the flexible member") {
  CHECK_EQ(sizeof(struct log_msg), 8u);
  CHECK_EQ(offsetof(struct log_msg, text), 6u);
}

TEST("one message round-trips") {
  log_reset();
  const struct log_msg *m = log_append(100, 2, "hello");
  REQUIRE(m != NULL);
  CHECK_EQ(m->tick, 100u);
  CHECK_EQ(m->severity, 2u);
  CHECK_EQ(m->len, 5u);
  CHECK_MEM_EQ(m->text, "hello", 5);
}

TEST("consecutive messages do not overlap") {
  log_reset();
  REQUIRE(log_append(100, 2, "hello") != NULL);
  REQUIRE(log_append(200, 1, "world") != NULL);

  const struct log_msg *first = log_get(0);
  REQUIRE(first != NULL);
  CHECK_EQ(first->tick, 100u);
  CHECK_EQ(first->len, 5u);
  CHECK_MEM_EQ(first->text, "hello", 5);

  const struct log_msg *second = log_get(1);
  REQUIRE(second != NULL);
  CHECK_EQ(second->tick, 200u);
  CHECK_MEM_EQ(second->text, "world", 5);
}

TEST("the arena holds exactly what the arithmetic says") {
  log_reset();
  // Each entry: align_up_4(6 + 10) = 16 bytes; 256 / 16 = 16 messages, the
  // sixteenth ending exactly at the arena's last byte.
  for (uint32_t i = 0; i < 16; ++i) {
    CHECK(log_append(i, 0, "0123456789") != NULL);
  }
  CHECK(log_append(99, 0, "0123456789") == NULL);
  CHECK_EQ(log_count(), 16u);
}
