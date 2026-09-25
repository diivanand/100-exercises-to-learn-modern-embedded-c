// Solution -- 04.07 String tables: names in flash, not in RAM

#include <mect/mect.h>

#include <stddef.h>

enum fault_code {
  FAULT_NONE = 0,
  FAULT_OVERVOLT,
  FAULT_UNDERVOLT,
  FAULT_OVERTEMP,
  FAULT_SENSOR_LOST,
  FAULT_COUNT // keep last: it is the table's size and the range check
};

// Both consts earn their keep. `const char *` makes the CHARACTERS
// read-only; the second const makes the POINTERS read-only too, which is
// what lets the whole table live in .rodata -- flash -- instead of costing
// 4 bytes of RAM per entry at boot. Designated initialisers (01.04) tie
// each slot to its enumerator by NAME, so reordering the enum cannot
// silently misalign the table.
static const char *const fault_names[] = {
    [FAULT_NONE] = "NONE",
    [FAULT_OVERVOLT] = "OVERVOLT",
    [FAULT_UNDERVOLT] = "UNDERVOLT",
    [FAULT_OVERTEMP] = "OVERTEMP",
    [FAULT_SENSOR_LOST] = "SENSOR_LOST",
};

// The table and the enum can still drift apart -- a new fault with no
// name. Make that a compile error instead of a wild read (07.05 dwells
// on _Static_assert; 07.06's X-macros remove even this duplication).
_Static_assert(sizeof fault_names / sizeof fault_names[0] == FAULT_COUNT,
               "every fault_code needs a name");

const char *fault_name(enum fault_code code) {
  // The cast handles negative values with the same single comparison:
  // (unsigned)-1 is huge, so it fails the bound check too (02.02).
  if ((unsigned)code >= (unsigned)FAULT_COUNT) {
    return "?";
  }
  return fault_names[code];
}

TEST("every fault has a name") {
  CHECK_EQ(fault_name(FAULT_NONE), "NONE");
  CHECK_EQ(fault_name(FAULT_OVERVOLT), "OVERVOLT");
  CHECK_EQ(fault_name(FAULT_UNDERVOLT), "UNDERVOLT");
  CHECK_EQ(fault_name(FAULT_OVERTEMP), "OVERTEMP");
  CHECK_EQ(fault_name(FAULT_SENSOR_LOST), "SENSOR_LOST");
}

TEST("out of range gets a placeholder, not a wild read") {
  CHECK_EQ(fault_name(FAULT_COUNT), "?");
  CHECK_EQ(fault_name((enum fault_code)99), "?");
  CHECK_EQ(fault_name((enum fault_code)-1), "?");
}
