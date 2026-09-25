// =============================================================================
//  09.02 -- Out-parameters: the result travels by pointer, untouched on
//           failure
// =============================================================================
//
//  When the return value carries STATUS (09.01), the RESULT travels through
//  a pointer parameter. The shape is everywhere in C APIs:
//
//      enum cfg_status config_get_u16(const char *key, uint16_t *out);
//
//  Conventions worth adopting wholesale:
//
//   - inputs first, outputs last, so call sites read left to right;
//   - on ANY failure, *out is LEFT UNTOUCHED. This is the contract that
//     makes the caller's code clean: set your default, call, use the value.
//     No failure branch has to repair the variable:
//
//         uint16_t timeout = 250;               // the fallback
//         (void)config_get_u16("timeout_ms", &timeout);
//         // timeout is now the config value, or still the fallback
//
//   - a NULL out pointer means "I only want the status" -- existence
//     checks come for free if you let them (03.01 set this policy).
//
//  THE STARTER breaks all three. It "helpfully" zeroes *out on entry --
//  which clobbers the caller's default AND crashes the NULL caller (CERT
//  EXP34-C) -- and it writes a PARTIAL result before its range check, so a
//  failed call leaves a truncated 115200 in a uint16_t. A caller who
//  checked the status is punished anyway: their fallback is gone.
//
//  Half-written outputs are how failures spread. The function that failed
//  was handled; the variable it scribbled on is used forty lines later.
//
//  TASK
//    Restore the contract: nothing is written unless the call succeeds,
//    and NULL is a legal way to say "status only". Do not change the tests.
//
//  RUN IT
//    ./mec test 09_02
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum cfg_status {
  CFG_OK = 0,
  CFG_ERR_NOT_FOUND,
  CFG_ERR_RANGE,
};

// The stored values are wider than the type callers ask for, because the
// store does not know who will ask (the real one parses text from flash).
static const struct {
  const char *key;
  int32_t value;
} store[] = {
    {"node_id", 42},
    {"timeout_ms", 500},
    {"baud", 115200}, // legitimate entry; does not FIT in a uint16_t
    {"offset", -12},
};

enum cfg_status config_get_u16(const char *key, uint16_t *out) {
  *out = 0; // TODO: "defensive", it said. Ask the caller with a default how
            // defended they feel. Ask the NULL caller.
  for (size_t i = 0; i < sizeof store / sizeof store[0]; ++i) {
    if (strcmp(store[i].key, key) != 0) {
      continue;
    }
    *out = (uint16_t)store[i].value; // TODO: written before the range check
    if (store[i].value < 0 || store[i].value > UINT16_MAX) {
      return CFG_ERR_RANGE;
    }
    return CFG_OK;
  }
  return CFG_ERR_NOT_FOUND;
}

TEST("a present, in-range key fills the out-parameter") {
  uint16_t value = 0;
  CHECK_EQ(config_get_u16("node_id", &value), CFG_OK);
  CHECK_EQ(value, 42u);
}

TEST("a missing key leaves the caller's default alone") {
  uint16_t timeout = 250; // the caller's carefully chosen fallback
  CHECK_EQ(config_get_u16("gain", &timeout), CFG_ERR_NOT_FOUND);
  CHECK_EQ(timeout, 250u);
}

TEST("an out-of-range value reports, and writes nothing") {
  uint16_t baud_divider = 35; // fallback computed for the default clock
  CHECK_EQ(config_get_u16("baud", &baud_divider), CFG_ERR_RANGE);
  CHECK_EQ(baud_divider, 35u);

  uint16_t offset = 7;
  CHECK_EQ(config_get_u16("offset", &offset), CFG_ERR_RANGE);
  CHECK_EQ(offset, 7u);
}

TEST("NULL out asks only whether the key exists and fits") {
  CHECK_EQ(config_get_u16("timeout_ms", NULL), CFG_OK);
  CHECK_EQ(config_get_u16("gain", NULL), CFG_ERR_NOT_FOUND);
}
