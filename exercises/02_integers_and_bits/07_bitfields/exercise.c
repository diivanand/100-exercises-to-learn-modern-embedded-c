// =============================================================================
//  02.07 -- Bit numbering, and why the register byte is built by hand
// =============================================================================
//
//  A radio's configuration register, from its datasheet:
//
//      | 7      | 6..4    | 3..1  | 0   |
//      | ENABLE | CHANNEL | POWER | ACK |
//
//  Datasheets draw registers MSB-FIRST: the leftmost box is bit 7, the 128s
//  place. Bit 0 is always the least significant bit -- the 1s place. Reading
//  the figure left-to-right as "first field starts at bit 0" produces a
//  perfectly mirrored layout, and that is exactly what the starter did.
//
//  Notice which test catches it. The ROUND-TRIP test (encode, then decode)
//  PASSES in the starter -- a mirrored encoder and a mirrored decoder agree
//  with each other completely. Only the EXACT BYTES from the datasheet's
//  examples can convict them. When you write codecs, always test against
//  known wire values from the spec, never only against your own inverse.
//
//  WHY NOT BITFIELDS? C has syntax that looks made for this:
//
//      struct radio_config {
//          unsigned ack : 1; unsigned power : 3;
//          unsigned channel : 3; unsigned enable : 1;
//      };
//
//  Do not use it for wire formats or hardware registers. The standard makes
//  the allocation order of bitfields within a unit IMPLEMENTATION-DEFINED
//  (C17 6.7.2.1p11) -- whether `ack` is bit 0 or bit 7 is the compiler's
//  choice, as are padding and whether fields straddle unit boundaries.
//  MISRA C:2012 constrains bitfields sharply for the same reason, and every
//  BSP vendor builds register values with masks and shifts. Bitfields are
//  fine for PRIVATE, in-RAM flags where layout is invisible; the moment the
//  bits leave the program -- over a wire, into a register -- lay them out
//  yourself. (Chapter 11 builds full register definitions this way.)
//
//  TASK
//    Fix the bit positions in the encoder AND the decoders, using the
//    datasheet figure above. Do not change the tests.
//
//  RUN IT
//    ./mec test 02_07
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

// TODO: these positions transcribe the figure MIRRORED -- as if the leftmost
// box in the drawing were bit 0.
enum {
  CFG_ENABLE = 1u << 0,   //
  CFG_CHANNEL_POS = 1,    //
  CFG_CHANNEL_MASK = 0x7, //
  CFG_POWER_POS = 4,      //
  CFG_POWER_MASK = 0x7,   //
  CFG_ACK = 1u << 7,      //
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
