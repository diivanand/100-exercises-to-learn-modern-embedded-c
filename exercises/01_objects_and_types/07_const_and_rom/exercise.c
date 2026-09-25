// =============================================================================
//  01.07 -- const: a promise to the compiler, a home in flash
// =============================================================================
//
//  NOTE: this exercise starts as a COMPILE ERROR -- two of them, both const
//  violations. One is even reported inside a test; the fix is still in the
//  function SIGNATURES, not the tests.
//
//  On a desktop, const is about catching mistakes. On an MCU it is also
//  about REAL ESTATE. The NUCLEO board this course's bonus track targets has
//  1 MB of flash and 128 KB of RAM: an eight-to-one ratio. A file-scope
//  object the compiler can prove never changes goes to .rodata, which the
//  linker places in flash; drop the const and it moves to .data, which costs
//  RAM -- plus a boot-time copy (06.04). A 4 KB calibration table is
//  invisible as const and 3% of all RAM without it.
//
//  The habits, then (Effective C ch. 2 on type qualifiers, ch. 7 on string
//  literals):
//
//   - TABLES ARE `static const`. Lookup tables, calibration data, fonts,
//     strings: const by default, RAM by exception.
//   - POINTER PARAMETERS ARE `const T *` UNLESS THE FUNCTION WRITES. This is
//     the promise callers rely on -- and the gate that lets const objects
//     flow in at all. One non-const parameter deep in a call chain forces
//     the const off everything above it ("const poisoning" runs bottom-up).
//   - STRING LITERALS ARE READ-ONLY. Their type is technically char[N] for
//     K&R-era reasons, but writing to one is undefined behaviour (CERT
//     STR30-C) -- on the target they are flash, and flash ignores stray
//     writes; on the host you get SIGBUS. This course builds with
//     -Wwrite-strings, which gives literals the const type they always
//     deserved; `char *s = "OVERTEMP"` is an error here.
//   - const BEATS #define FOR DATA. A const object has a type, a scope, an
//     address you can pass around, and a place in the debugger. A macro has
//     none of those (chapter 07 gives macros their due where they earn it).
//
//  Below, an alarm-label function whose return type disowns the promise its
//  string literals need, and a checksum routine that demands write access
//  it never uses -- which the const calibration table in the tests refuses
//  to grant.
//
//  TASK
//    Fix both signatures. The bodies are already correct. Do not change the
//    tests.
//
//  RUN IT
//    ./mec test 01_07
//
// =============================================================================

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// TODO: these literals are read-only; the return type says otherwise.
char *alarm_label(uint8_t code) {
  switch (code) {
  case 1:
    return "OVERTEMP";
  case 2:
    return "UNDERVOLT";
  default:
    return "UNKNOWN";
  }
}

// TODO: reads only, but demands the right to write.
uint8_t table_checksum(uint8_t *table, size_t n) {
  uint8_t x = 0;
  for (size_t i = 0; i < n; ++i) {
    x = (uint8_t)(x ^ table[i]);
  }
  return x;
}

TEST("alarm codes have labels") {
  CHECK_EQ(alarm_label(1), "OVERTEMP");
  CHECK_EQ(alarm_label(2), "UNDERVOLT");
  CHECK_EQ(alarm_label(99), "UNKNOWN");
}

TEST("a const calibration table can be checksummed in place") {
  // File-scope const: this is the kind of object that lives in flash. If
  // table_checksum demanded a mutable pointer, the choice would be a
  // diagnostic or a RAM copy -- both wrong.
  static const uint8_t CAL_TABLE[] = {0x10, 0x20, 0x30, 0x40};
  CHECK_EQ(table_checksum(CAL_TABLE, sizeof CAL_TABLE), 0x40u);
  CHECK_EQ(table_checksum(CAL_TABLE, 0), 0u);
}
