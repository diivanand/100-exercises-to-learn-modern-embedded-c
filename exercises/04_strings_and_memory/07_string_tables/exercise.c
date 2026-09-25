// =============================================================================
//  04.07 -- String tables: names in flash, not in RAM
// =============================================================================
//
//  Firmware needs names for things: fault codes in the log, states on the
//  debug console, commands in the help text. The C shape for "enum in,
//  string out" is a table of pointers -- and the details decide whether it
//  is trustworthy and where the bytes live.
//
//  WHERE THE BYTES LIVE. The string literals themselves are always in
//  .rodata (flash, 04.01). But the table of POINTERS is an object too:
//
//      static const char *table[]        -- pointers are writable: RAM
//      static const char *const table[]  -- pointers const too: flash
//
//  Miss the second const and the linker quietly places the array in
//  .data: on an MCU that is 4 bytes of RAM per entry, plus startup copy
//  time, for a table nobody ever writes to. A log subsystem's format
//  strings can be one of the larger single RAM savings on a 128 KB part.
//  (06.04 maps out .data/.bss/.rodata properly.)
//
//  STAYING IN SYNC. A positional initialiser list is four names that all
//  hope the enum never changes. Two upgrades below the fold:
//
//   - DESIGNATED INITIALISERS (01.04): `[FAULT_OVERTEMP] = "OVERTEMP"`
//     binds each slot to its enumerator by name.
//   - A COUNT SENTINEL plus _Static_assert (07.05): keep FAULT_COUNT as
//     the last enumerator and the compiler itself proves the table is
//     complete. (07.06's X-macros generate enum AND table from one list,
//     removing the duplication altogether.)
//
//  BOUNDS. `fault_names[code]` trusts every caller forever. Fault codes
//  come from logs, from flash records written by older firmware, from the
//  wire -- a stale or corrupt value walks straight off the table and
//  returns whatever bytes live there, if it returns at all. A lookup
//  guards its range and hands back a placeholder; one unsigned cast makes
//  a single comparison cover negative values too (02.02). Chapter 09
//  turns this instinct into API policy.
//
//  TASK
//    Fix the table (complete it, move it to flash, tie it to the enum) and
//    guard `fault_name`. Do not change the tests.
//
//  RUN IT
//    ./mec test 04_07      (the asan build names the wild read precisely)
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>

enum fault_code {
  FAULT_NONE = 0,
  FAULT_OVERVOLT,
  FAULT_UNDERVOLT,
  FAULT_OVERTEMP,
  FAULT_SENSOR_LOST,
  FAULT_COUNT // keep last
};

// TODO: one entry short (that slot is a NULL pointer), pointers in RAM,
// and nothing keeps this aligned with the enum above.
static const char *fault_names[FAULT_COUNT] = {
    "NONE",
    "OVERVOLT",
    "UNDERVOLT",
    "OVERTEMP",
};

const char *fault_name(enum fault_code code) {
  // TODO: no bounds check -- FAULT_COUNT and anything stale or corrupt
  // reads past the table (the asan build names it; without asan it may
  // even crash mid-test, which is its own lesson in diagnosability).
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
