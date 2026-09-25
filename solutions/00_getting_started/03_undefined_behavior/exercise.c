// Solution -- 00.03 Undefined behaviour, and why "it works" proves nothing

#include <mect/mect.h>

#include <stdint.h>
#include <string.h>

int flash_table_midpoint(int lo, int hi) {
  // Difference first: hi - lo cannot overflow when lo <= hi, and adding half
  // of it back to lo stays within [lo, hi]. This is CERT INT32-C's example,
  // and it is the form every binary search should use.
  return lo + (hi - lo) / 2;
}

void wipe_key(uint8_t *key, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    key[i] = 0;
  }
  // (A production wipe_key has a second problem this course returns to in
  // 11.01: with no use after the store, the optimiser may delete the whole
  // loop. memset_s exists for this; so does a volatile pointer.)
}

TEST("midpoint of small values") {
  CHECK_EQ(flash_table_midpoint(0, 10), 5);
  CHECK_EQ(flash_table_midpoint(7, 8), 7);
}

TEST("midpoint of large values does not overflow") {
  CHECK_EQ(flash_table_midpoint(2000000000, 2100000000), 2050000000);
}

TEST("wipe_key clears the key and nothing else") {
  struct {
    uint8_t key[16];
    uint8_t canary; // sits right after the buffer; must survive
  } slot;
  memset(slot.key, 0xAB, sizeof slot.key);
  slot.canary = 0x5A;

  wipe_key(slot.key, sizeof slot.key);

  for (size_t i = 0; i < sizeof slot.key; ++i) {
    CHECK_EQ(slot.key[i], 0u);
  }
  CHECK_EQ(slot.canary, 0x5Au); // the canary is the point of this test
}
