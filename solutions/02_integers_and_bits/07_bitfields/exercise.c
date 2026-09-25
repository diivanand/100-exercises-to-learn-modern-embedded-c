// Solution -- 02.07 Bit numbering, and why the register byte is built by hand

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// The datasheet's layout, transcribed as named masks and positions. Bit 0
// is the LSB -- the 1s place -- regardless of which end the figure draws
// first. Writing the shift amounts down ONCE, next to the datasheet names,
// is the whole defence.
enum {
  CFG_ENABLE = 1u << 7,   // bit 7
  CFG_CHANNEL_POS = 4,    // bits 6..4
  CFG_CHANNEL_MASK = 0x7, //
  CFG_POWER_POS = 1,      // bits 3..1
  CFG_POWER_MASK = 0x7,   //
  CFG_ACK = 1u << 0,      // bit 0
};

uint8_t radio_config_byte(bool enable, unsigned channel, unsigned power, bool ack) {
  uint8_t byte = 0;
  if (enable) {
    byte |= CFG_ENABLE;
  }
  byte = (uint8_t)(byte | ((channel & CFG_CHANNEL_MASK) << CFG_CHANNEL_POS));
  byte = (uint8_t)(byte | ((power & CFG_POWER_MASK) << CFG_POWER_POS));
  if (ack) {
    byte |= CFG_ACK;
  }
  return byte;
}

unsigned radio_config_channel(uint8_t byte) {
  return (byte >> CFG_CHANNEL_POS) & CFG_CHANNEL_MASK;
}

unsigned radio_config_power(uint8_t byte) {
  return (byte >> CFG_POWER_POS) & CFG_POWER_MASK;
}

TEST("the exact wire bytes from the datasheet's examples") {
  // enable=1, channel=5, power=3, ack=1:
  //   1 101 011 1 = 0xD7
  CHECK_EQ(radio_config_byte(true, 5, 3, true), 0xD7u);
  // disabled, channel 0, power 7, no ack: 0 000 111 0 = 0x0E
  CHECK_EQ(radio_config_byte(false, 0, 7, false), 0x0Eu);
  // everything off is all zeros
  CHECK_EQ(radio_config_byte(false, 0, 0, false), 0x00u);
}

TEST("decode round-trips encode") {
  const uint8_t byte = radio_config_byte(true, 6, 2, false);
  CHECK_EQ(radio_config_channel(byte), 6u);
  CHECK_EQ(radio_config_power(byte), 2u);
}
