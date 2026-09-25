// Solution -- 05.06 Flexible array members: one struct, many sizes

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
// alignment for whoever comes next. offsetof, NOT sizeof -- sizeof is 8
// (it includes trailing padding for array duty) while the text really
// starts at 6. sizeof would waste 2 bytes per entry.
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
  // Advance by the same arithmetic the walker strides by. Writer and reader
  // sharing ONE size formula is the invariant; entry_size() is that formula.
  log_arena_used += need;
  ++log_msg_count;
  return m;
}

const struct log_msg *log_get(size_t index) {
  if (index >= log_msg_count) {
    return NULL;
  }
  size_t off = 0;
  for (size_t i = 0; i < index; ++i) {
    const struct log_msg *m =
        (const struct log_msg *)(const void *)&log_arena[off];
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
