// Solution -- 03.02 Array decay: the [16] in a parameter list is a comment

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Entries, not bytes -- and it keeps being right when the table grows.
// Guard rail: this macro must only ever see a REAL ARRAY. Applied to a
// pointer it compiles happily and answers nonsense (8 / element size).
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

// The honest signature: a pointer and a length, decided by the caller. The
// starter's `uint8_t key[16]` promised nothing -- a parameter declared as an
// array IS a pointer (C17 6.7.6.3p7), and sizeof it is sizeof a pointer.
void wipe_key(uint8_t *key, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    key[i] = 0;
  }
}

static const uint16_t thresholds[] = {150, 300, 450, 600, 4095};

size_t threshold_count(void) {
  return ARRAY_SIZE(thresholds);
}

uint16_t threshold_at(size_t i) {
  // The count and the bound come from the same macro, so they cannot drift.
  if (i >= ARRAY_SIZE(thresholds)) {
    i = ARRAY_SIZE(thresholds) - 1;
  }
  return thresholds[i];
}

TEST("wipe_key clears the whole key") {
  uint8_t key[16];
  memset(key, 0xAB, sizeof key);
  wipe_key(key, sizeof key); // sizeof works HERE: key is a real array here
  for (size_t i = 0; i < sizeof key; ++i) {
    CHECK_EQ(key[i], 0u);
  }
}

TEST("wipe_key respects the length it is given") {
  uint8_t buf[8];
  memset(buf, 0xAB, sizeof buf);
  wipe_key(buf, 4);
  CHECK_EQ(buf[3], 0u);
  CHECK_EQ(buf[4], 0xABu); // beyond len: untouched
}

TEST("the table reports entries, not bytes") {
  CHECK_EQ(threshold_count(), 5u);
  CHECK_EQ(threshold_at(0), 150u);
  CHECK_EQ(threshold_at(99), 4095u); // clamped by the same ARRAY_SIZE
}
