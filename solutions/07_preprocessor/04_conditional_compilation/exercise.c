// Solution -- 07.04 Conditional compilation without the folklore

#include <mect/mect.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Stands in for the build system's -DCFG_FRAME_CHECKSUM=1.
#define CFG_FRAME_CHECKSUM 1

// The guard: a missing (or misspelt) configuration name is a BUILD ERROR,
// not a silently-compiled-out feature. Every consumer of the flag below can
// now use plain #if, knowing the name is defined to something.
#if !defined(CFG_FRAME_CHECKSUM)
#error "CFG_FRAME_CHECKSUM must be defined to 0 or 1 by the build config"
#endif

static bool checksum_enabled(void) {
#if CFG_FRAME_CHECKSUM
  return true;
#else
  return false;
#endif
}

// Encode a payload for the wire; with the checksum feature on, append the
// XOR of the payload bytes.
static size_t frame_encode(const uint8_t *payload, size_t len, uint8_t *out) {
  for (size_t i = 0; i < len; ++i) {
    out[i] = payload[i];
  }
#if CFG_FRAME_CHECKSUM
  uint8_t x = 0;
  for (size_t i = 0; i < len; ++i) {
    x ^= payload[i];
  }
  out[len] = x;
  return len + 1;
#else
  return len;
#endif
}

TEST("the feature the build asked for is actually compiled in") {
  CHECK(checksum_enabled());
}

TEST("an encoded frame carries its checksum byte") {
  const uint8_t payload[] = {0x10, 0x20, 0x33};
  uint8_t out[8];
  CHECK_EQ(frame_encode(payload, sizeof payload, out), (size_t)4);
  CHECK_EQ(out[0], 0x10u);
  CHECK_EQ(out[3], 0x03u); // 0x10 ^ 0x20 ^ 0x33
}

TEST("a one-byte payload checksums to itself") {
  const uint8_t payload[] = {0xAB};
  uint8_t out[4];
  CHECK_EQ(frame_encode(payload, sizeof payload, out), (size_t)2);
  CHECK_EQ(out[1], 0xABu);
}
