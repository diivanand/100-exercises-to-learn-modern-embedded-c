// =============================================================================
//  10.06 -- Characterisation: pin the legacy down before you touch it
// =============================================================================
//
//  READ THIS FIRST: in every other exercise in this course, the tests are
//  right and the code is wrong. THIS EXERCISE IS THE OTHER WAY AROUND. The
//  code below is legacy -- shipped in 2011, parsing EEPROM config strings
//  in three products -- and for legacy code, WHAT IT DOES IS THE SPEC,
//  warts included. You will fix the TESTS to match the code.
//
//  Grenning's legacy-change algorithm (ch. 13) starts here: before changing
//  code that has no tests, write CHARACTERISATION TESTS -- tests that
//  assert what the code currently does, learned by running it, not by
//  reading what you wish it did. They are a tripwire, not an endorsement:
//  once the behaviour is pinned, you can refactor and know within seconds
//  whether you changed something some deployed device depends on. (The
//  same move under the name "learning tests" is how you pin down a vendor
//  SDK you do not control.)
//
//  Two tests below have expectations somebody GUESSED instead of measured
//  -- reasonable guesses, both wrong, which is rather the point:
//
//   1. Duplicate keys: "surely the first value wins, or it is an error."
//      Find out what actually happens to `baud`.
//   2. Unknown keys: "surely they are not counted in the return value."
//      Find out what the function actually returns.
//
//  Work them out by reading parse_legacy_config -- then confirm by running.
//  `./mec test 10_06` shows you left-vs-right for every failure, which IS
//  the measurement. Correct the expectations; leave the function alone.
//
//  When you are done, notice what you have: a safety net woven from the
//  code's own oddities. The moment it is green, refactoring stops being
//  archaeology and becomes engineering.
//
//  TASK
//    Fix the two guessed expectations so the suite documents what the
//    legacy parser really does. Do not "improve" parse_legacy_config.
//
//  RUN IT
//    ./mec test 10_06
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// --- legacy_config.c -- DO NOT MODIFY ---------------------------------------------
//
// Shipped in 2011. Three products parse their EEPROM strings with it. Its
// warts are load-bearing until proven otherwise.

struct legacy_config {
  long baud;
  long retries;
  bool echo;
};

static int parse_legacy_config(const char *text, struct legacy_config *out) {
  int processed = 0;
  const char *p = text;
  while (*p != '\0') {
    const char *eq = p;
    while (*eq != '\0' && *eq != '=' && *eq != ';') {
      ++eq;
    }
    if (*eq != '=') { // no '=': skip the fragment without counting it
      p = (*eq == ';') ? eq + 1 : eq;
      continue;
    }
    const size_t key_len = (size_t)(eq - p);
    long value = 0;
    const char *v = eq + 1;
    while (*v >= '0' && *v <= '9') {
      value = value * 10 + (*v - '0');
      ++v;
    }
    if (key_len == 4 && strncmp(p, "baud", 4) == 0) {
      out->baud = value;
    } else if (key_len == 7 && strncmp(p, "retries", 7) == 0) {
      out->retries = value;
    } else if (key_len == 4 && strncmp(p, "echo", 4) == 0) {
      out->echo = (value != 0);
    }
    ++processed; // unknown keys fall through above -- but still count
    while (*v != '\0' && *v != ';') {
      ++v;
    }
    p = (*v == ';') ? v + 1 : v;
  }
  return processed;
}

// --- characterisation suite ---------------------------------------------------------

TEST("the happy path everyone remembers") {
  struct legacy_config cfg = {0};
  CHECK_EQ(parse_legacy_config("baud=9600;retries=3;echo=1", &cfg), 3);
  CHECK_EQ(cfg.baud, 9600);
  CHECK_EQ(cfg.retries, 3);
  CHECK(cfg.echo);
}

TEST("characterise: duplicate keys") {
  struct legacy_config cfg = {0};
  CHECK_EQ(parse_legacy_config("baud=9600;baud=115200", &cfg), 2);
  // TODO: a guess, not a measurement. Which write wins? Read the loop, run
  // the test, and record what the code DOES.
  CHECK_EQ(cfg.baud, 9600);
}

TEST("characterise: unknown keys") {
  struct legacy_config cfg = {0};
  // TODO: also a guess. Does an unknown key count towards the return
  // value? The answer defines what the return value MEANS.
  CHECK_EQ(parse_legacy_config("color=red;baud=19200", &cfg), 1);
  CHECK_EQ(cfg.baud, 19200);
  CHECK_EQ(cfg.retries, 0);
}

TEST("characterise: the warts nobody would design today") {
  struct legacy_config cfg = {0};
  // No trimming: " baud" is a different (unknown) key. Counted, ignored.
  CHECK_EQ(parse_legacy_config(" baud=9600", &cfg), 1);
  CHECK_EQ(cfg.baud, 0);

  // Digits stop at the first non-digit; the rest of the value vanishes.
  CHECK_EQ(parse_legacy_config("baud=96k00", &cfg), 1);
  CHECK_EQ(cfg.baud, 96);

  // An empty value parses as 0 -- so "echo=" quietly means echo OFF.
  cfg.echo = true;
  CHECK_EQ(parse_legacy_config("echo=", &cfg), 1);
  CHECK_FALSE(cfg.echo);
}
