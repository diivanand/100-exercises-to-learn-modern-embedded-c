// Solution -- 10.06 Characterisation: pin the legacy down before you touch it

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
  // Ran it: the LAST write wins. Not "first wins", not an error. Some
  // deployed EEPROM string out there almost certainly relies on appending
  // an override to the end -- which is why we write this down instead of
  // "fixing" it.
  CHECK_EQ(cfg.baud, 115200);
}

TEST("characterise: unknown keys") {
  struct legacy_config cfg = {0};
  // Ran it: an unknown key is silently ignored -- but COUNTED. The return
  // value is "pairs seen", not "pairs applied". Callers that compare the
  // return against an expected count are (accidentally) tolerant of typos.
  CHECK_EQ(parse_legacy_config("color=red;baud=19200", &cfg), 2);
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

// With the behaviour pinned, the first refactor I would risk: extract the
// digit-scan into a named helper and give it an explicit "junk after the
// digits" policy. Any deviation now fails one of the tests above -- which
// is the entire point of writing them first (Grenning ch. 13).
