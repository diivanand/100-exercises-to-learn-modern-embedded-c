// Solution -- 04.01 Strings are arrays, and the NUL is not optional

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

size_t label_storage_bytes(const char *label) {
  // strlen counts the characters; the storage also holds the terminator.
  return strlen(label) + 1;
}

bool label_copy(char *dst, size_t dst_size, const char *src) {
  // `>=` is the whole fix: a string of exactly dst_size characters needs
  // dst_size + 1 bytes. (Phrased this way round -- rather than
  // `strlen(src) + 1 > dst_size` -- there is no +1 to overflow, pedantic as
  // that is for a string length.)
  if (strlen(src) >= dst_size) {
    return false;
  }
  strcpy(dst, src); // fine HERE: the bound was just proven, not hoped for
  return true;
}

TEST("storage counts the terminator") {
  CHECK_EQ(label_storage_bytes("PUMP0"), 6u);
  CHECK_EQ(label_storage_bytes(""), 1u);
}

TEST("sizeof measures the array, strlen counts to the NUL") {
  const char banner[] = "boot";
  CHECK_EQ(sizeof banner, 5u); // four characters and the terminator
  CHECK_EQ(strlen(banner), 4u);
}

TEST("a label that fits, fits") {
  struct {
    char slot[8];
    char canary; // sits right after the slot; must survive every copy
  } s;
  s.canary = 0x5A;
  CHECK(label_copy(s.slot, sizeof s.slot, "PUMP0"));
  CHECK_EQ(s.slot, "PUMP0");
  CHECK_EQ(s.canary, 0x5A);
}

TEST("a label of exactly the slot size does NOT fit") {
  struct {
    char slot[8];
    char canary;
  } s;
  s.canary = 0x5A;
  // Eight characters need nine bytes. Accepting this writes the terminator
  // one past the end of the slot -- straight into the canary.
  CHECK_FALSE(label_copy(s.slot, sizeof s.slot, "PRESSURE"));
  CHECK_EQ(s.canary, 0x5A);
}

TEST("four characters need five bytes") {
  struct {
    char slot[4];
    char canary;
  } s;
  s.canary = 0x77;
  // Four characters, four bytes, no room for the NUL: refuse.
  CHECK_FALSE(label_copy(s.slot, sizeof s.slot, "FAN1"));
  CHECK_EQ(s.canary, 0x77);
}
