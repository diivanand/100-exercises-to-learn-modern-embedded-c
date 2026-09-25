// =============================================================================
//  03.01 -- Out-parameters and the NULL contract
// =============================================================================
//
//  C returns one value. When a function has more to say -- a status AND a
//  result, or two results -- the C mechanism is the OUT-PARAMETER: the
//  caller passes a pointer, the function writes through it. Half of every
//  embedded API you will ever use is shaped exactly like the function below:
//  return a status, deliver results through pointers.
//
//  That shape comes with two contracts, and both are the API designer's job:
//
//  1. WHAT MAY BE NULL. If an output is optional, say so and check for it --
//     once, at the boundary. If a pointer must not be NULL, document that
//     instead; a policy of "every function re-checks every pointer, every
//     time" bloats a small flash image and turns real bugs into silently
//     skipped work. CERT EXP34-C (do not dereference null pointers) is
//     satisfied by a checked boundary, not by paranoia everywhere.
//     (Chapter 09 returns to this when APIs grow error codes.)
//
//  2. WHAT HAPPENS ON FAILURE. Write the outputs only after the input has
//     been validated. A function that fails AND has already scribbled on the
//     caller's variables leaves the caller with a value that looks real and
//     is not -- firmware that "reads version 0.65535 from erased flash" and
//     happily proceeds is this exact bug.
//
//  THE PROBLEM AT HAND. A product's version is programmed into the last word
//  of flash: major in the top 16 bits, minor in the bottom 16. Flash that
//  was never programmed reads as ALL ONES -- 0xFFFFFFFF is the erased state
//  of NOR flash, a fact you will meet again and again. `version_split`
//  rejects an erased word, and `minor` is documented as optional.
//
//  The starter dereferences `minor` unconditionally -- the second test
//  passes NULL and CRASHES. A crash is a test failure like any other here;
//  re-run under the sanitizer (cmake --preset asan) and it names the line.
//  It also writes both outputs before checking for the erased pattern, which
//  the third test can see.
//
//  TASK
//    Fix `version_split`: validate first, honour the NULL contract.
//    Do not change the tests.
//
//  RUN IT
//    ./mec test 03_01
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Splits a version word from flash. Returns false if the word is erased
// (0xFFFFFFFF). `major` and `minor` may each be NULL if the caller does not
// need that half. Outputs are written only on success.
bool version_split(uint32_t reg, unsigned *major, unsigned *minor) {
  // TODO: this writes before validating, and through NULL without checking.
  *major = reg >> 16;
  *minor = reg & 0xFFFFu;
  if (reg == UINT32_MAX) {
    return false;
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
