// =============================================================================
//  09.06 -- Validate at the boundary, and validate ALL of it
// =============================================================================
//
//  Where do the checks go? Not everywhere -- a 4 MHz loop cannot afford
//  every function re-validating what its caller already proved, and
//  paranoia scattered through a code base is how real checks get lost in
//  the noise. The rule:
//
//   - AT TRUST BOUNDARIES -- bytes off a wire, sensor readings, anything a
//     human typed -- validation is TOTAL. Every value is guilty until
//     proven in range.
//   - BETWEEN FRIENDLY MODULES, a broken precondition is a bug, and bugs
//     get asserts (09.05), not error returns nobody will ever handle.
//
//  This exercise is one trust boundary done properly: a command byte and a
//  value byte arrive off the UART, and `command_apply` decides what they
//  are allowed to touch.
//
//  The starter has the two classic boundary failures:
//
//   1. OFF-BY-ONE RANGES. The spec says value 50..200 INCLUSIVE; the code
//      says `value > 50 && value < 200`, quietly outlawing both endpoints.
//      Boundary values are where range bugs live, so boundary values are
//      what the tests probe -- 49, 50, 200, 201.
//
//   2. AN INCOMPLETE COMMAND CHECK. Someone "left room for future
//      commands" by accepting the whole 0x10..0x1F block, and the set
//      handler happily indexes four channels with a nibble that reaches
//      15. The write lands past the array -- here on a canary, on your
//      board on whatever the linker put next.
//
//  Note the shape of the last test: it does not sample a few bad inputs,
//  it tries ALL 256 command bytes. At a trust boundary the input space is
//  usually small enough to enumerate -- so enumerate it. (This is the
//  poor man's fuzzing, and for one byte it is complete.)
//
//  TASK
//    Make the bounds inclusive as specified, and reject every command
//    outside 0x10..0x13 and 0x20 -- BEFORE using it as an index. Rejected
//    inputs must change nothing. Do not change the tests.
//
//  RUN IT
//    ./mec test 09_06
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

enum cmd_status {
  CMD_OK = 0,
  CMD_ERR_UNKNOWN,
  CMD_ERR_RANGE,
};

// The wire protocol under test:
//   0x10..0x13  set brightness of channel 0..3; value must be 50..200
//   0x20        ping (value ignored)
//   everything else is unknown and must change NOTHING.
#define CMD_SET_BASE 0x10
#define CMD_SET_LAST 0x13
#define CMD_PING 0x20
#define VALUE_MIN 50
#define VALUE_MAX 200

static struct {
  uint8_t channels[4];
  uint8_t canary[32]; // the memory "after the channels" (00.03's trick)
} state;

static void state_reset(void) {
  for (unsigned i = 0; i < sizeof state.channels; ++i) {
    state.channels[i] = 0;
  }
  for (unsigned i = 0; i < sizeof state.canary; ++i) {
    state.canary[i] = 0xA5;
  }
}

enum cmd_status command_apply(uint8_t cmd, uint8_t value) {
  if (cmd == CMD_PING) {
    return CMD_OK;
  }
  // TODO: "room for future commands", says the comment this line came
  // with. The index below reaches channel 15 of 4.
  if (cmd < CMD_SET_BASE || cmd > 0x1F) {
    return CMD_ERR_UNKNOWN;
  }
  // TODO: the spec says 50..200 inclusive.
  if (!(value > VALUE_MIN && value < VALUE_MAX)) {
    return CMD_ERR_RANGE;
  }
  state.channels[cmd - CMD_SET_BASE] = value;
  return CMD_OK;
}

TEST("valid commands land on the right channel") {
  state_reset();
  CHECK_EQ(command_apply(0x10, 100), CMD_OK);
  CHECK_EQ(command_apply(0x13, 51), CMD_OK);
  CHECK_EQ(state.channels[0], 100u);
  CHECK_EQ(state.channels[3], 51u);
  CHECK_EQ(command_apply(0x20, 0), CMD_OK); // ping ignores its value
}

TEST("the range bounds are inclusive") {
  state_reset();
  CHECK_EQ(command_apply(0x11, 49), CMD_ERR_RANGE);
  CHECK_EQ(command_apply(0x11, 50), CMD_OK); // the minimum is legal
  CHECK_EQ(state.channels[1], 50u);
  CHECK_EQ(command_apply(0x11, 200), CMD_OK); // so is the maximum
  CHECK_EQ(state.channels[1], 200u);
  CHECK_EQ(command_apply(0x11, 201), CMD_ERR_RANGE);
  CHECK_EQ(state.channels[1], 200u); // rejected values change nothing
}

TEST("every possible command byte, because the wire will send them all") {
  state_reset();
  for (unsigned c = 0; c <= 0xFF; ++c) {
    const uint8_t cmd = (uint8_t)c;
    const enum cmd_status st = command_apply(cmd, 100);
    if ((cmd >= CMD_SET_BASE && cmd <= CMD_SET_LAST) || cmd == CMD_PING) {
      CHECK_EQ(st, CMD_OK);
    } else {
      CHECK_EQ(st, CMD_ERR_UNKNOWN);
    }
  }
  // Nothing above may have written past the four real channels.
  for (unsigned i = 0; i < sizeof state.canary; ++i) {
    CHECK_EQ(state.canary[i], 0xA5u);
  }
}
