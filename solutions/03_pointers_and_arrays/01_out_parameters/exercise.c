// Solution -- 03.01 Out-parameters and the NULL contract

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool version_split(uint32_t reg, unsigned *major, unsigned *minor) {
  // Validate FIRST. Nothing is written until the input is known good, so a
  // caller's variables are never left half-updated by a failed call.
  if (reg == UINT32_MAX) {
    return false;
  }
  // Either out may be NULL: "I do not care about this one" is part of the
  // documented contract, and it is checked here, at the API boundary --
  // not re-checked at every use inside (CERT EXP34-C is about the boundary).
  if (major != NULL) {
    *major = reg >> 16;
  }
  if (minor != NULL) {
    *minor = reg & 0xFFFFu;
  }
  return true;
}

TEST("splits a programmed version register") {
  unsigned major = 0;
  unsigned minor = 0;
  CHECK(version_split(0x00010007u, &major, &minor));
  CHECK_EQ(major, 1u);
  CHECK_EQ(minor, 7u);
}

TEST("minor is optional: NULL means the caller does not care") {
  unsigned major = 0;
  CHECK(version_split(0x00030002u, &major, NULL));
  CHECK_EQ(major, 3u);
}

TEST("erased flash is rejected and the outputs are left untouched") {
  unsigned major = 42;
  unsigned minor = 42;
  CHECK_FALSE(version_split(0xFFFFFFFFu, &major, &minor));
  CHECK_EQ(major, 42u); // a failed call must not tear the caller's state
  CHECK_EQ(minor, 42u);
}
