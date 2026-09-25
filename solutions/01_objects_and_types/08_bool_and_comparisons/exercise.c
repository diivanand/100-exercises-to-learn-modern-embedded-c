// Solution -- 01.08 bool, truthiness, and the comparison that lies

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#define STATUS_HEATER 0x01u
#define STATUS_READY 0x04u
#define STATUS_ERROR 0x80u

bool link_ready(uint8_t status) {
  // Test for NONZERO, never for equality with 1. The masked value is 0x04
  // when the bit is set; it will never be 1. `!= 0` states the question the
  // hardware is actually being asked.
  return (status & STATUS_READY) != 0;
}

bool heater_on(uint8_t status) {
  // Identical shape. The starter's `== 1` HAPPENED to work here because
  // this mask is bit 0 -- which is exactly how the broken pattern gets
  // copy-pasted onto a mask where it fails.
  return (status & STATUS_HEATER) != 0;
}

uint8_t error_flag(uint8_t status) {
  // A wire field documented "0 or 1" wants normalising, not just masking.
  // `!!` is the classic spelling; `(status & STATUS_ERROR) ? 1 : 0` says
  // the same thing more slowly. Converting to bool also normalises: the
  // conversion is defined as `!= 0`, not truncation -- (bool)0x80 is true.
  return (uint8_t)!!(status & STATUS_ERROR);
}

TEST("ready is a bit test, not an equality with 1") {
  CHECK(link_ready(0x04));
  CHECK(link_ready(0xFF));
  CHECK_FALSE(link_ready(0x00));
  CHECK_FALSE(link_ready(0xFB)); // everything BUT ready
}

TEST("the same pattern on bit 0, where the broken version got lucky") {
  CHECK(heater_on(0x01));
  CHECK(heater_on(0x05));
  CHECK_FALSE(heater_on(0xFE));
}

TEST("the error field is exactly 0 or 1 on the wire") {
  CHECK_EQ(error_flag(0x80), 1u);
  CHECK_EQ(error_flag(0xFF), 1u);
  CHECK_EQ(error_flag(0x7F), 0u);
}

TEST("bool conversion is a zero test, not a truncation") {
  // (uint8_t)0x100 is 0 -- narrowing keeps the low bits. (bool)0x100 is
  // true -- the conversion asks "is it nonzero?". Different questions.
  CHECK_EQ((uint8_t)0x100, 0u);
  CHECK_EQ((bool)0x100, true);
}
