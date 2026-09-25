// Solution -- 09.02 Out-parameters: the result travels by pointer, untouched
// on failure

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
  for (size_t i = 0; i < sizeof store / sizeof store[0]; ++i) {
    if (strcmp(store[i].key, key) != 0) {
      continue;
    }
    if (store[i].value < 0 || store[i].value > UINT16_MAX) {
      return CFG_ERR_RANGE; // *out not touched: the caller's default survives
    }
    if (out != NULL) { // NULL means "just tell me whether it exists and fits"
      *out = (uint16_t)store[i].value;
    }
    return CFG_OK;
  }
  return CFG_ERR_NOT_FOUND; // *out not touched here either
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
