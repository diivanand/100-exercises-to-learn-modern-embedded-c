// =============================================================================
//  04.01 -- Strings are arrays, and the NUL is not optional
// =============================================================================
//
//  C has no string type. A "string" is a bare array of char with a zero byte
//  -- the NUL terminator -- marking the end, and every string function in
//  the library is a convention built on that byte. Two consequences matter
//  daily (Effective C ch. 7):
//
//   1. LENGTH IS COUNTED, NOT STORED. strlen() walks the bytes until it
//      finds the 0, and its answer does NOT include the 0 -- but the
//      STORAGE has to. "PUMP0" needs six bytes. Forgetting the +1 is the
//      canonical off-by-one of C, and it does not fail politely: the copy
//      that overruns by one byte lands on whatever sits next in memory.
//      The tests below put a canary there (00.03's trick) so you can watch
//      it die. CERT STR31-C: guarantee that storage for strings has
//      sufficient space for the data AND the terminator.
//
//   2. sizeof MEASURES THE OBJECT, strlen COUNTS THE CHARACTERS. For
//      `const char banner[] = "boot"`, sizeof banner is 5 -- the array that
//      the literal initialised, terminator included -- while strlen(banner)
//      is 4. (sizeof only works where the array type is visible; hand the
//      array to a function and it decays to a pointer. 03.02 dwells on
//      that trap.)
//
//  WHERE LITERALS LIVE. A string literal is an array in read-only storage:
//  .rodata, which on an MCU means flash. `char *p = "LIVE";` points p
//  straight at flash, and writing p[0] is undefined behaviour -- on a
//  desktop it faults, on a Cortex-M it silently does nothing, both are
//  "correct". This build refuses the declaration outright: -Wwrite-strings
//  gives literals the type `const char *`. By contrast
//  `char a[] = "LIVE";` declares an ordinary writable array that is
//  INITIALISED from the literal -- the literal stays in flash, the array
//  costs five bytes of RAM, and writing a[0] is fine.
//
//  TASK
//    Fix `label_storage_bytes` and `label_copy`. Do not change the tests.
//
//  RUN IT
//    ./mec test 04_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// How many bytes of storage does a copy of `label` need?
size_t label_storage_bytes(const char *label) {
  // TODO: one byte short.
  return strlen(label);
}

// Copy `src` into a slot of `dst_size` bytes -- but only if it fits,
// terminator included. Returns false (and writes nothing) otherwise.
bool label_copy(char *dst, size_t dst_size, const char *src) {
  // TODO: this check forgets the terminator. A src of exactly dst_size
  // characters is accepted, and strcpy writes its NUL one byte past the
  // end of dst.
  if (strlen(src) > dst_size) {
    return false;
  }
  strcpy(dst, src);
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
