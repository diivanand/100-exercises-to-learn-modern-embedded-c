// =============================================================================
//  03.02 -- Array decay: the [16] in a parameter list is a comment
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR. Read the messages; both of
//  them are pointing at the lesson.
//
//  In almost every expression, an array DECAYS: its name converts to a
//  pointer to its first element. You know this from K&R. The places it does
//  NOT decay are worth listing, because they are exactly where `sizeof`
//  keeps working:
//
//      uint8_t key[16];
//      sizeof key        // 16       -- no decay inside sizeof
//      &key              // uint8_t (*)[16], a pointer to the ARRAY
//      wipe(key)         // decays: the function receives uint8_t *
//
//  And one place it LOOKS like it does not, but does: a function parameter.
//
//      void wipe_key(uint8_t key[16])   // [16] is DOCUMENTATION. The
//                                       // parameter is uint8_t *key.
//                                       // (C17 6.7.6.3p7; CERT ARR01-C)
//
//  Inside that function, `sizeof key` is sizeof a POINTER -- 8 on your Mac,
//  4 on the Cortex-M4 -- and a loop bounded by it clears half a key here and
//  a quarter of it on the target. This bug ships. The compiler now has a
//  diagnostic dedicated to it, which is the error you are looking at.
//
//  The rule that falls out: A FUNCTION THAT TAKES AN ARRAY TAKES A LENGTH.
//  The (pointer, len) pair travels together; the tests call the function
//  that way, and the starter's signature has to change to match.
//
//  For arrays that have NOT decayed -- file-scope tables, locals -- compute
//  the count from the array itself instead of repeating a number that will
//  rot when the table grows:
//
//      #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
//
//  `threshold_count` below returns the size in BYTES (10, not 5): sizeof on
//  a uint16_t table counts uint16_ts twice each. Fix it with the macro. And
//  remember what the macro must never see: a pointer. It would compile, and
//  answer 4, silently. (C23 finally added a real countof; until your
//  toolchain has it, discipline is the tool.)
//
//  TASK
//    Give `wipe_key` its honest (pointer, length) signature and fix its
//    loop; define ARRAY_SIZE and fix both threshold functions with it.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 03_02
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Crypto hygiene again (00.03) -- but this version has TWO bugs the compiler
// can see. TODO: fix the signature, then the loop bound.
void wipe_key(uint8_t key[16]) {
  for (size_t i = 0; i < sizeof key; ++i) {
    key[i] = 0;
  }
}

static const uint16_t thresholds[] = {150, 300, 450, 600, 4095};

size_t threshold_count(void) {
  // TODO: bytes, not entries.
  return sizeof thresholds;
}

uint16_t threshold_at(size_t i) {
  // TODO: same disease as above -- this clamp is in BYTES, so indexes 5..9
  // sail straight past the end of a five-entry table.
  if (i >= sizeof thresholds) {
    i = sizeof thresholds - 1;
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
