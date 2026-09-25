// Solution -- 01.07 const: a promise to the compiler, a home in flash

#include <mect/mect.h>

#include <stddef.h>
#include <stdint.h>

// const char *: the caller gets a pointer it may read through and nothing
// more. The literals themselves sit in .rodata -- in flash, on the target.
const char *alarm_label(uint8_t code) {
  switch (code) {
  case 1:
    return "OVERTEMP";
  case 2:
    return "UNDERVOLT";
  default:
    return "UNKNOWN";
  }
}

// const on the parameter is API documentation the compiler enforces: this
// function reads the table, full stop. Without it, no const table -- and
// calibration tables want to BE const, so they live in flash instead of
// eating RAM -- could ever be passed in without a diagnostic.
uint8_t table_checksum(const uint8_t *table, size_t n) {
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
